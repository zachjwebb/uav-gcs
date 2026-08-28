#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <uavlink/packet.h>

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
    
    printf("Pass: test_byte_order\n");
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
    
    printf("Pass: test_reserved_byte\n");
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
    
    printf("Pass: test_rejection\n");
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

    printf("Pass: test_telemetry_round_trip\n");

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

    printf("Pass: test_telemetry_layout\n");
}

int main(void) {
    printf("=== Starting UAVLink Packet Tests ===\n");
    
    test_round_trip();
    test_byte_order();
    test_reserved_byte();
    test_rejection();
    test_telemetry_round_trip();
    test_telemetry_wire_layout();

    if (failures != 0) {
        fprintf(stderr, "=== %d check(s) FAILED ===\n", failures);
        return 1;
    }
    
    printf("=== All UAVLink Packet Tests Passed Successfully ===\n");
    return 0;
}