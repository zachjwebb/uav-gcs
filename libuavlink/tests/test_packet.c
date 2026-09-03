#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <uavlink/packet.h>
#include <limits.h>
#include <uavlink/crc.h>

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

/* ---- Telemetry test fixture: every field distinct ---- */
static uavlink_telemetry_t make_telemetry(void) {
    uavlink_telemetry_t tm = {
        .timestamp_ms    = 123456789,
        .latitude        = 451234567,
        .longitude       = -1229876543,
        .altitude_amsl   = 12345,
        .ground_speed    = 1550,
        .vertical_speed  = -250,
        .heading         = 27050,
        .roll            = -1500,
        .pitch           = 500,
        .battery_voltage = 22200,
        .battery_pct     = 87,
        .gps_fix_type    = UAVLINK_GPS_FIX_3D,
        .gps_sat_count   = 14,
        .flight_mode     = UAVLINK_MODE_AUTO,
        .status_flags    = UAVLINK_STATUS_ARMED
    };
    return tm;
}

/* Test 1: Round-trip validation 
 * Builds a valid header, encodes it, decodes it, and CHECKs that 
 * all four fields perfectly match the original values. */
void test_round_trip(void) {
    uavlink_header_t original_hdr = {
        .version = UAVLINK_VERSION,
        .msg_type = UAVLINK_MSG_TELEMETRY,
        .seq = 0xABCDE123,
        .payload_len = 100
    };
    
    uint8_t buf[UAVLINK_MAX_PACKET];
    memset(buf, 0xAA, sizeof(buf)); // Fill buffer with dummy data
    
    // 1. Encode the header
    uavlink_result_t encode_res = uavlink_encode_header(&original_hdr, buf, sizeof(buf));
    CHECK(encode_res == UAVLINK_OK);
    
    // 2. Decode the header (Cast first macro to size_t to enforce unsigned arithmetic evaluation)
    uavlink_header_t decoded_hdr;
    size_t expected_packet_len = (size_t)UAVLINK_HEADER_SIZE + original_hdr.payload_len + UAVLINK_CRC_SIZE;
    uavlink_result_t decode_res = uavlink_decode_header(buf, expected_packet_len, &decoded_hdr);
    CHECK(decode_res == UAVLINK_OK);
    
    // 3. CHECK all fields match perfectly
    CHECK(decoded_hdr.version == original_hdr.version);
    CHECK(decoded_hdr.msg_type == original_hdr.msg_type);
    CHECK(decoded_hdr.seq == original_hdr.seq);
    CHECK(decoded_hdr.payload_len == original_hdr.payload_len);
    
    printf("Pass: test_round_trip\n");
}

/* Test 2: Byte-order verification (FMT-004)
 * Ensures sequence number is strictly serialized as Big-Endian bytes, 
 * catching potential host-endianness shortcuts or erroneous platform macros. */
void test_byte_order(void) {
    uavlink_header_t hdr = {
        .version = UAVLINK_VERSION,
        .msg_type = UAVLINK_MSG_HEARTBEAT,
        .seq = 0x01020304,
        .payload_len = 10
    };
    
    uint8_t buf[UAVLINK_MAX_PACKET];
    uavlink_result_t res = uavlink_encode_header(&hdr, buf, sizeof(buf));
    CHECK(res == UAVLINK_OK);
    
    // CHECK Big-Endian ordering explicitly by index
    CHECK(buf[2] == 0x01);
    CHECK(buf[3] == 0x02);
    CHECK(buf[4] == 0x03);
    CHECK(buf[5] == 0x04);
    
}

/* Test 3: Reserved byte asymmetric handling
 * Verifies that encode enforces a 0x00 clean default on byte 7, 
 * but decode remains resilient and passes even if incoming data contains altered noise. */
void test_reserved_byte(void) {
    uavlink_header_t hdr = {
        .version = UAVLINK_VERSION,
        .msg_type = UAVLINK_MSG_COMMAND,
        .seq = 999,
        .payload_len = 25
    };
    
    uint8_t buf[UAVLINK_MAX_PACKET];
    uavlink_result_t res = uavlink_encode_header(&hdr, buf, sizeof(buf));
    CHECK(res == UAVLINK_OK);
    
    // CHECK byte 7 is written cleanly as 0x00
    CHECK(buf[7] == 0x00);
    
    // Simulate asymmetric network mutation by altering the reserved byte 
    buf[7] = 0xFF;
    
    // Verify that decode ignores byte 7 and executes successfully
    uavlink_header_t decoded_hdr;
    size_t expected_packet_len = (size_t)UAVLINK_HEADER_SIZE + hdr.payload_len + UAVLINK_CRC_SIZE;
    res = uavlink_decode_header(buf, expected_packet_len, &decoded_hdr);
    CHECK(res == UAVLINK_OK);
    CHECK(decoded_hdr.version == 1);
    
}

/* Test 4: Rejection and Error Bounds testing
 * Confirms that illegal properties (version mismatch, out-of-bounds message types, 
 * payload sizes exceeding payload ceiling, or truncated packet delivery lengths) 
 * match their exact designated failure symbols. */
void test_rejection(void) {
    uint8_t buf[UAVLINK_MAX_PACKET];
    uavlink_header_t hdr;
    uavlink_header_t dummy_decode;
    
    // 1. Invalid version rejection (FMT-001)
    hdr = (uavlink_header_t){ .version = 2, .msg_type = UAVLINK_MSG_TELEMETRY, .seq = 1, .payload_len = 10 };
    CHECK(uavlink_encode_header(&hdr, buf, sizeof(buf)) == UAVLINK_ERR_VERSION);
    
    // 2. Out-of-bounds message type rejection (FMT-006)
    hdr = (uavlink_header_t){ .version = 1, .msg_type = 0, .seq = 1, .payload_len = 10 };
    CHECK(uavlink_encode_header(&hdr, buf, sizeof(buf)) == UAVLINK_ERR_MSG_TYPE);
    hdr = (uavlink_header_t){ .version = 1, .msg_type = 6, .seq = 1, .payload_len = 10 };
    CHECK(uavlink_encode_header(&hdr, buf, sizeof(buf)) == UAVLINK_ERR_MSG_TYPE);
    
    // 3. Oversized payload allocation tracking (FMT-002)
    hdr = (uavlink_header_t){ .version = 1, .msg_type = UAVLINK_MSG_TELEMETRY, .seq = 1, .payload_len = UAVLINK_MAX_PAYLOAD + 1 };
    CHECK(uavlink_encode_header(&hdr, buf, sizeof(buf)) == UAVLINK_ERR_LENGTH);
    
    // 4. Truncated verification length validation
    hdr = (uavlink_header_t){ .version = 1, .msg_type = UAVLINK_MSG_TELEMETRY, .seq = 1, .payload_len = 50 };
    CHECK(uavlink_encode_header(&hdr, buf, sizeof(buf)) == UAVLINK_OK);
    
    // Expected complete envelope: 8 (header) + 50 (payload) + 2 (crc) = 60 bytes.
    // Supplying 59U bytes should throw a truncated status code error.
    uavlink_result_t truncate_res = uavlink_decode_header(buf, 59U, &dummy_decode);
    CHECK(truncate_res == UAVLINK_ERR_TRUNCATED);
    
}

/* Test 5: Telemetry round-trip.
 * Every field holds a distinct value so that a swapped-offset bug
 * cannot round-trip cleanly. Four fields are negative to exercise
 * the signed -> unsigned -> signed conversion path. */
void test_telemetry_round_trip(void) {

    uavlink_telemetry_t original = make_telemetry();
    uint8_t buf[UAVLINK_TELEMETRY_PAYLOAD_SIZE];

    CHECK(uavlink_encode_telemetry(&original, buf, sizeof(buf)) == UAVLINK_OK);
    

    uavlink_telemetry_t decoded;
    CHECK(uavlink_decode_telemetry(buf, sizeof(buf), &decoded) == UAVLINK_OK);

    CHECK(decoded.timestamp_ms    == original.timestamp_ms);
    CHECK(decoded.latitude        == original.latitude);
    CHECK(decoded.longitude       == original.longitude);
    CHECK(decoded.altitude_amsl   == original.altitude_amsl);
    CHECK(decoded.ground_speed    == original.ground_speed);
    CHECK(decoded.vertical_speed  == original.vertical_speed);
    CHECK(decoded.heading         == original.heading);
    CHECK(decoded.roll            == original.roll);
    CHECK(decoded.pitch           == original.pitch);
    CHECK(decoded.battery_voltage == original.battery_voltage);
    CHECK(decoded.battery_pct     == original.battery_pct);
    CHECK(decoded.gps_fix_type    == original.gps_fix_type);
    CHECK(decoded.gps_sat_count   == original.gps_sat_count);
    CHECK(decoded.flight_mode     == original.flight_mode);
    CHECK(decoded.status_flags    == original.status_flags);

}

/* Test 6: Wire offsets and byte order, checked against ICD section 3
 * rather than against the encoder's own behaviour. Distinctive values
 * are chosen so each byte position is unambiguous. */
void test_telemetry_wire_layout(void) {

    uavlink_telemetry_t tm = make_telemetry();
    tm.timestamp_ms  = 0x01020304;
    tm.latitude      = 0x05060708;
    tm.longitude     = 0x090A0B0C;
    tm.altitude_amsl = 0x0D0E0F10;
    tm.ground_speed  = 0x1112;

    uint8_t buf[UAVLINK_TELEMETRY_PAYLOAD_SIZE];
    CHECK(uavlink_encode_telemetry(&tm, buf, sizeof(buf)) == UAVLINK_OK);

    /* timestamp_ms at offset 0 */
    CHECK(buf[0] == 0x01); CHECK(buf[1] == 0x02);
    CHECK(buf[2] == 0x03); CHECK(buf[3] == 0x04);

    /* latitude at offset 4 */
    CHECK(buf[4] == 0x05); CHECK(buf[5] == 0x06);
    CHECK(buf[6] == 0x07); CHECK(buf[7] == 0x08);

    /* longitude at offset 8 -- NOT altitude */
    CHECK(buf[8]  == 0x09); CHECK(buf[9]  == 0x0A);
    CHECK(buf[10] == 0x0B); CHECK(buf[11] == 0x0C);

    /* altitude_amsl at offset 12 */
    CHECK(buf[12] == 0x0D); CHECK(buf[13] == 0x0E);
    CHECK(buf[14] == 0x0F); CHECK(buf[15] == 0x10);

    /* ground_speed at offset 16 */
    CHECK(buf[16] == 0x11); CHECK(buf[17] == 0x12);

    /* single-byte fields */
    CHECK(buf[28] == 87);
    CHECK(buf[29] == UAVLINK_GPS_FIX_3D);
    CHECK(buf[30] == 14);
    CHECK(buf[31] == UAVLINK_MODE_AUTO);
    CHECK(buf[32] == UAVLINK_STATUS_ARMED);

}

/* Test 7: Signed extremes. Two's-complement conversion bugs surface
 * at the limits, not at ordinary values. */
void test_telemetry_signed_extremes(void) {

    uavlink_telemetry_t tm = make_telemetry();
    uint8_t buf[UAVLINK_TELEMETRY_PAYLOAD_SIZE];
    uavlink_telemetry_t decoded;

    tm.latitude = -1;
    tm.longitude = INT32_MIN;
    tm.altitude_amsl = INT32_MAX;
    tm.roll = INT16_MIN;
    tm.pitch = INT16_MAX;
    tm.vertical_speed = -1;

    CHECK(uavlink_encode_telemetry(&tm, buf, sizeof(buf)) == UAVLINK_OK);
    CHECK(uavlink_decode_telemetry(buf, sizeof(buf), &decoded) == UAVLINK_OK);

    CHECK(decoded.latitude == -1);
    CHECK(decoded.longitude == INT32_MIN);
    CHECK(decoded.altitude_amsl == INT32_MAX);
    CHECK(decoded.roll == INT16_MIN);
    CHECK(decoded.pitch == INT16_MAX);
    CHECK(decoded.vertical_speed == -1);

    CHECK(buf[4] == 0xFF);
    CHECK(buf[5] == 0xFF);
    CHECK(buf[6] == 0xFF);
    CHECK(buf[7] == 0xFF);

}

/* Test 8: Buffer bounds and NULL arguments.
 * Exact-size buffers must succeed; one byte short must fail. */
void test_telemetry_bounds(void) {

    uavlink_telemetry_t tm = make_telemetry();
    uavlink_telemetry_t decoded;
    uint8_t exact[UAVLINK_TELEMETRY_PAYLOAD_SIZE];
    uint8_t small[UAVLINK_TELEMETRY_PAYLOAD_SIZE - 1];

    CHECK(uavlink_encode_telemetry(&tm, exact, sizeof(exact)) == UAVLINK_OK);
    CHECK(uavlink_decode_telemetry(exact, sizeof(exact), &decoded) == UAVLINK_OK);

    CHECK(uavlink_encode_telemetry(&tm, small, sizeof(small)) == UAVLINK_ERR_BUFFER_TOO_SMALL);
    CHECK(uavlink_decode_telemetry(exact, sizeof(small), &decoded) == UAVLINK_ERR_BUFFER_TOO_SMALL);

    CHECK(uavlink_encode_telemetry(NULL, exact, sizeof(exact)) == UAVLINK_ERR_NULL);
    CHECK(uavlink_encode_telemetry(&tm, NULL, sizeof(exact))   == UAVLINK_ERR_NULL);
    CHECK(uavlink_decode_telemetry(NULL, sizeof(exact), &decoded) == UAVLINK_ERR_NULL);
    CHECK(uavlink_decode_telemetry(exact, sizeof(exact), NULL)    == UAVLINK_ERR_NULL);

}

/* Test 9: Protocol enum rejection, and the commit-on-success invariant:
 * a failed decode must leave the caller's struct untouched. */
void test_telemetry_enum_rejection(void) {

    uavlink_telemetry_t tm = make_telemetry();
    uint8_t buf[UAVLINK_TELEMETRY_PAYLOAD_SIZE];

    /* encode side */
    tm.flight_mode = UAVLINK_MODE_LANDED + 1;
    CHECK(uavlink_encode_telemetry(&tm, buf, sizeof(buf)) == UAVLINK_ERR_ENUM);

    tm = make_telemetry();
    tm.gps_fix_type = UAVLINK_GPS_FIX_RTK + 1;
    CHECK(uavlink_encode_telemetry(&tm, buf, sizeof(buf)) == UAVLINK_ERR_ENUM);

    /* decode side: corrupt a valid packet */
    tm = make_telemetry();
    CHECK(uavlink_encode_telemetry(&tm, buf, sizeof(buf)) == UAVLINK_OK);

    uavlink_telemetry_t out;
    memset(&out, 0x5A, sizeof(out));

    buf[31] = 200;   /* invalid flight_mode */
    CHECK(uavlink_decode_telemetry(buf, sizeof(buf), &out) == UAVLINK_ERR_ENUM);
    CHECK(out.latitude == (int32_t)0x5A5A5A5A);   /* untouched */

    buf[31] = UAVLINK_MODE_AUTO;
    buf[29] = 99;    /* invalid gps_fix_type */
    CHECK(uavlink_decode_telemetry(buf, sizeof(buf), &out) == UAVLINK_ERR_ENUM);
    CHECK(out.latitude == (int32_t)0x5A5A5A5A);

}

static uavlink_command_t make_command(void) {
    uavlink_command_t c = {
        .session_id = 0xDEADBEEF,
        .cmd_type   = UAVLINK_CMD_GOTO_WAYPOINT,
        .param1     = 451234567,
        .param2     = -1229876543,
        .param3     = 12345
    };
    return c;
}

static uavlink_ack_t make_ack(void) {
    uavlink_ack_t a = {
        .session_id  = 0xDEADBEEF,
        .ack_seq     = 77,
        .reason_code = UAVLINK_ACK_ACCEPTED
    };
    return a;
}

/* Test 10: full-packet round-trip, all four message types.
 * Expected totals come from ICD section 9. */
static void test_packet_round_trip(void) {
    uint8_t buf[UAVLINK_MAX_PACKET];
    size_t len = 0;
    uavlink_packet_t out;

    /* TELEMETRY -- 43 bytes */
    uavlink_packet_t tm_pkt;
    memset(&tm_pkt, 0, sizeof(tm_pkt));
    tm_pkt.header.msg_type = UAVLINK_MSG_TELEMETRY;
    tm_pkt.header.seq      = 1001;
    tm_pkt.payload.telemetry = make_telemetry();

    CHECK(uavlink_encode_packet(&tm_pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    CHECK(len == 43);
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_OK);
    CHECK(out.header.msg_type == UAVLINK_MSG_TELEMETRY);
    CHECK(out.header.seq == 1001);
    CHECK(out.header.payload_len == UAVLINK_TELEMETRY_PAYLOAD_SIZE);
    CHECK(out.payload.telemetry.latitude == tm_pkt.payload.telemetry.latitude);
    CHECK(out.payload.telemetry.longitude == tm_pkt.payload.telemetry.longitude);
    CHECK(out.payload.telemetry.flight_mode == tm_pkt.payload.telemetry.flight_mode);

    /* COMMAND -- 27 bytes */
    uavlink_packet_t cmd_pkt;
    memset(&cmd_pkt, 0, sizeof(cmd_pkt));
    cmd_pkt.header.msg_type = UAVLINK_MSG_COMMAND;
    cmd_pkt.header.seq      = 5;
    cmd_pkt.payload.command = make_command();

    CHECK(uavlink_encode_packet(&cmd_pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    CHECK(len == 27);
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_OK);
    CHECK(out.payload.command.session_id == 0xDEADBEEF);
    CHECK(out.payload.command.cmd_type == UAVLINK_CMD_GOTO_WAYPOINT);
    CHECK(out.payload.command.param2 == -1229876543);

    /* HEARTBEAT -- 10 bytes */
    uavlink_packet_t hb_pkt;
    memset(&hb_pkt, 0, sizeof(hb_pkt));
    hb_pkt.header.msg_type = UAVLINK_MSG_HEARTBEAT;
    hb_pkt.header.seq      = 42;

    CHECK(uavlink_encode_packet(&hb_pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    CHECK(len == 10);
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_OK);
    CHECK(out.header.payload_len == 0);
    CHECK(out.header.seq == 42);

    /* ACK and NACK -- 19 bytes each */
    uavlink_packet_t ack_pkt;
    memset(&ack_pkt, 0, sizeof(ack_pkt));
    ack_pkt.header.msg_type = UAVLINK_MSG_ACK;
    ack_pkt.header.seq      = 9;
    ack_pkt.payload.ack     = make_ack();

    CHECK(uavlink_encode_packet(&ack_pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    CHECK(len == 19);
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_OK);
    CHECK(out.payload.ack.ack_seq == 77);

    ack_pkt.header.msg_type = UAVLINK_MSG_NACK;
    ack_pkt.payload.ack.reason_code = UAVLINK_ACK_ILLEGAL_TRANSITION;
    CHECK(uavlink_encode_packet(&ack_pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    CHECK(len == 19);
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_OK);
    CHECK(out.header.msg_type == UAVLINK_MSG_NACK);
    CHECK(out.payload.ack.reason_code == UAVLINK_ACK_ILLEGAL_TRANSITION);
}

/* Test 11: CRC covers header AND payload (FMT-003). */
static void test_packet_crc_rejection(void) {
    uint8_t buf[UAVLINK_MAX_PACKET];
    size_t len = 0;
    uavlink_packet_t out;

    uavlink_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.header.msg_type = UAVLINK_MSG_TELEMETRY;
    pkt.header.seq      = 1;
    pkt.payload.telemetry = make_telemetry();

    /* payload corruption */
    CHECK(uavlink_encode_packet(&pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    buf[20] ^= 0x01;
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_ERR_CRC);

    /* header corruption -- fails if CRC covered payload only */
    CHECK(uavlink_encode_packet(&pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    buf[3] ^= 0x01;
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_ERR_CRC);

    /* CRC field itself corrupted */
    CHECK(uavlink_encode_packet(&pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    buf[len - 1] ^= 0xFF;
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_ERR_CRC);
}

/* Test 12: per-type payload length check (ICD 8.7 step 5).
 * CRC is recomputed so the length check is what fires, not the CRC check. */
static void test_packet_wrong_payload_len(void) {
    uint8_t buf[UAVLINK_MAX_PACKET];
    size_t len = 0;
    uavlink_packet_t out;

    uavlink_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.header.msg_type = UAVLINK_MSG_TELEMETRY;
    pkt.header.seq      = 1;
    pkt.payload.telemetry = make_telemetry();

    CHECK(uavlink_encode_packet(&pkt, buf, sizeof(buf), &len) == UAVLINK_OK);

    buf[6] = 20;
    uint16_t crc = uavlink_crc16(buf, UAVLINK_HEADER_SIZE + 20u);
    buf[UAVLINK_HEADER_SIZE + 20] = (uint8_t)(crc >> 8);
    buf[UAVLINK_HEADER_SIZE + 21] = (uint8_t)(crc & 0xFF);

    size_t forged_len = (size_t)UAVLINK_HEADER_SIZE + 20u + UAVLINK_CRC_SIZE;
    CHECK(uavlink_decode_packet(buf, forged_len, &out) == UAVLINK_ERR_LENGTH);
}

/* Test 13: truncation and trailing data. Run under ASan. */
static void test_packet_length_mismatch(void) {
    uint8_t buf[UAVLINK_MAX_PACKET];
    size_t len = 0;
    uavlink_packet_t out;

    uavlink_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.header.msg_type = UAVLINK_MSG_TELEMETRY;
    pkt.header.seq      = 1;
    pkt.payload.telemetry = make_telemetry();

    CHECK(uavlink_encode_packet(&pkt, buf, sizeof(buf), &len) == UAVLINK_OK);

    CHECK(uavlink_decode_packet(buf, len - 1, &out) == UAVLINK_ERR_TRUNCATED);
    CHECK(uavlink_decode_packet(buf, len + 1, &out) == UAVLINK_ERR_TRAILING_DATA);
    CHECK(uavlink_decode_packet(buf, 5, &out) == UAVLINK_ERR_BUFFER_TOO_SMALL);

    uint8_t tiny[UAVLINK_HEADER_SIZE];
    size_t tiny_len = 0;
    CHECK(uavlink_encode_packet(&pkt, tiny, sizeof(tiny), &tiny_len)
          == UAVLINK_ERR_BUFFER_TOO_SMALL);

    CHECK(uavlink_encode_packet(NULL, buf, sizeof(buf), &len) == UAVLINK_ERR_NULL);
    CHECK(uavlink_encode_packet(&pkt, NULL, sizeof(buf), &len) == UAVLINK_ERR_NULL);
    CHECK(uavlink_encode_packet(&pkt, buf, sizeof(buf), NULL) == UAVLINK_ERR_NULL);
    CHECK(uavlink_decode_packet(NULL, len, &out) == UAVLINK_ERR_NULL);
    CHECK(uavlink_decode_packet(buf, len, NULL) == UAVLINK_ERR_NULL);
}

/* Test 14: a rejected packet must not modify the caller's struct. */
static void test_packet_commit_on_success(void) {
    uint8_t buf[UAVLINK_MAX_PACKET];
    size_t len = 0;

    uavlink_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.header.msg_type = UAVLINK_MSG_TELEMETRY;
    pkt.header.seq      = 1;
    pkt.payload.telemetry = make_telemetry();

    CHECK(uavlink_encode_packet(&pkt, buf, sizeof(buf), &len) == UAVLINK_OK);
    buf[20] ^= 0x01;

    uavlink_packet_t out;
    memset(&out, 0x5A, sizeof(out));
    CHECK(uavlink_decode_packet(buf, len, &out) == UAVLINK_ERR_CRC);
    CHECK(out.header.seq == 0x5A5A5A5A);
    CHECK(out.payload.telemetry.latitude == (int32_t)0x5A5A5A5A);
}

int main(void) {
    printf("=== Starting UAVLink Packet Tests ===\n");
    
    test_round_trip();
    test_byte_order();
    test_reserved_byte();
    test_rejection();
    test_telemetry_round_trip();
    test_telemetry_wire_layout();
    test_telemetry_signed_extremes();
    test_telemetry_bounds();
    test_telemetry_enum_rejection();
    test_telemetry_enum_rejection();
    test_packet_round_trip();
    test_packet_crc_rejection();
    test_packet_wrong_payload_len();
    test_packet_length_mismatch();
    test_packet_commit_on_success();

    if (failures != 0) {
        fprintf(stderr, "=== %d check(s) FAILED ===\n", failures);
        return 1;
    }
    
    printf("=== All UAVLink Packet Tests Passed Successfully ===\n");
    return 0;
}