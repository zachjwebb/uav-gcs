#include <uavlink/packet.h>
#include <uavlink/crc.h>

static void put_u32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >>  8);
    b[3] = (uint8_t)(v);
}

static void put_u16(uint8_t *b, uint16_t v) {
    b[0] = (uint8_t)(v >> 8);
    b[1] = (uint8_t)(v);
}

static uint32_t get_u32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] <<  8) | ((uint32_t)b[3]);
}

static uint16_t get_u16(const uint8_t *b) {
    return (uint16_t)(((uint16_t)b[0] << 8) | ((uint16_t)b[1]));
}

/**
 * Internal helper to map message types to payload sizes.
 * @return Payload size in bytes, or -1 if the type is unrecognized.
 */
static int32_t get_payload_size(uavlink_msg_type_t msg_type) {
    switch (msg_type) {
        case UAVLINK_MSG_TELEMETRY: return UAVLINK_TELEMETRY_PAYLOAD_SIZE;
        case UAVLINK_MSG_COMMAND:   return UAVLINK_COMMAND_PAYLOAD_SIZE;
        case UAVLINK_MSG_HEARTBEAT: return UAVLINK_HEARTBEAT_PAYLOAD_SIZE;
        case UAVLINK_MSG_ACK:
        case UAVLINK_MSG_NACK:  return UAVLINK_ACK_PAYLOAD_SIZE;
        default: return -1;
    }
}

uavlink_result_t uavlink_encode_header(const uavlink_header_t *header, uint8_t *buf, size_t buf_len) {

    if (header == NULL || buf == NULL) {
        return UAVLINK_ERR_NULL;
    }

    if (buf_len < UAVLINK_HEADER_SIZE) {
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }

    if (header->version != UAVLINK_VERSION) {
        return UAVLINK_ERR_VERSION;
    }

    if (header->msg_type < UAVLINK_MSG_TELEMETRY || header->msg_type > UAVLINK_MSG_NACK) {
        return UAVLINK_ERR_MSG_TYPE;
    }

    if (header->payload_len > UAVLINK_MAX_PAYLOAD) {
        return UAVLINK_ERR_LENGTH;
    }

    buf[0] = header->version;
    buf[1] = header->msg_type;
    buf[2] = (uint8_t)((header->seq >> 24) & 0xFF);
    buf[3] = (uint8_t)((header->seq >> 16) & 0xFF);
    buf[4] = (uint8_t)((header->seq >> 8) & 0xFF);
    buf[5] = (uint8_t)(header->seq & 0xFF);

    buf[6] = header->payload_len;

    buf[7] = 0x00; //Reserved

    return UAVLINK_OK;
}

uavlink_result_t uavlink_decode_header(const uint8_t *buf, size_t buf_len, uavlink_header_t *header) {

    if (buf == NULL || header == NULL) {
        return UAVLINK_ERR_NULL;
    }

    if (buf_len < UAVLINK_HEADER_SIZE) {
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }

    uavlink_header_t tmp;
    tmp.version     = buf[0];
    tmp.msg_type    = buf[1];
    tmp.seq         = ((uint32_t)buf[2] << 24) | ((uint32_t)buf[3] << 16)
                    | ((uint32_t)buf[4] <<  8) | ((uint32_t)buf[5]);
    tmp.payload_len = buf[6];

    if (tmp.version != UAVLINK_VERSION) {
        return UAVLINK_ERR_VERSION;
    }
    if (tmp.msg_type < UAVLINK_MSG_TELEMETRY || tmp.msg_type > UAVLINK_MSG_NACK) {
        return UAVLINK_ERR_MSG_TYPE;
    }
    if (tmp.payload_len > UAVLINK_MAX_PAYLOAD) {
        return UAVLINK_ERR_LENGTH;
    }

    size_t declared = (size_t)UAVLINK_HEADER_SIZE + tmp.payload_len + UAVLINK_CRC_SIZE;
    if (declared > buf_len) {
        return UAVLINK_ERR_TRUNCATED;
    }
    if (declared < buf_len) {
        return UAVLINK_ERR_TRAILING_DATA;
    }

    *header = tmp;
    return UAVLINK_OK;
}

const char *uavlink_strerror(uavlink_result_t r) {
    switch (r) {
        case UAVLINK_OK:                   return "UAVLINK_OK";
        case UAVLINK_ERR_NULL:             return "UAVLINK_ERR_NULL";
        case UAVLINK_ERR_BUFFER_TOO_SMALL: return "UAVLINK_ERR_BUFFER_TOO_SMALL";
        case UAVLINK_ERR_VERSION:          return "UAVLINK_ERR_VERSION";
        case UAVLINK_ERR_MSG_TYPE:         return "UAVLINK_ERR_MSG_TYPE";
        case UAVLINK_ERR_LENGTH:           return "UAVLINK_ERR_LENGTH";
        case UAVLINK_ERR_CRC:              return "UAVLINK_ERR_CRC";
        case UAVLINK_ERR_TRUNCATED:        return "UAVLINK_ERR_TRUNCATED";
        case UAVLINK_ERR_TRAILING_DATA:    return "UAVLINK_ERR_TRAILING_DATA";
        case UAVLINK_ERR_ENUM:             return "UAVLINK_ERR_ENUM";
        default:                           return "UNKNOWN_UAVLINK_ERROR";
    }
}

uavlink_result_t uavlink_encode_telemetry(const uavlink_telemetry_t *tm, uint8_t *buf, size_t buf_len) {

    if (tm == NULL || buf == NULL) {
        return UAVLINK_ERR_NULL;
    }

    if (buf_len < UAVLINK_TELEMETRY_PAYLOAD_SIZE) {
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }

    if (tm->flight_mode > UAVLINK_MODE_LANDED) {
        return UAVLINK_ERR_ENUM;
    }

    if (tm->gps_fix_type > UAVLINK_GPS_FIX_RTK) {
        return UAVLINK_ERR_ENUM;
    }

    put_u32(&buf[0],  tm->timestamp_ms);
    put_u32(&buf[4],  (uint32_t)tm->latitude);
    put_u32(&buf[8],  (uint32_t)tm->longitude);
    put_u32(&buf[12], (uint32_t)tm->altitude_amsl);
    put_u16(&buf[16], tm->ground_speed);
    put_u16(&buf[18], (uint16_t)tm->vertical_speed);
    put_u16(&buf[20], tm->heading);
    put_u16(&buf[22], (uint16_t)tm->roll);
    put_u16(&buf[24], (uint16_t)tm->pitch);
    put_u16(&buf[26], tm->battery_voltage);
    buf[28] = tm->battery_pct;
    buf[29] = tm->gps_fix_type;
    buf[30] = tm->gps_sat_count;
    buf[31] = tm->flight_mode;
    buf[32] = tm->status_flags;

    return UAVLINK_OK;

}

uavlink_result_t uavlink_decode_telemetry(const uint8_t *buf, size_t buf_len, uavlink_telemetry_t *tm) {

    if (buf == NULL || tm == NULL) {
        return UAVLINK_ERR_NULL;
    }

    if (buf_len < UAVLINK_TELEMETRY_PAYLOAD_SIZE) {
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }

    uavlink_telemetry_t tmp;
    tmp.timestamp_ms = get_u32(&buf[0]);
    tmp.latitude = (int32_t)get_u32(&buf[4]);
    tmp.longitude = (int32_t)get_u32(&buf[8]);
    tmp.altitude_amsl = (int32_t)get_u32(&buf[12]);
    tmp.ground_speed = get_u16(&buf[16]);
    tmp.vertical_speed = (int16_t)get_u16(&buf[18]);
    tmp.heading = get_u16(&buf[20]);
    tmp.roll = (int16_t)get_u16(&buf[22]);
    tmp.pitch = (int16_t)get_u16(&buf[24]);
    tmp.battery_voltage = get_u16(&buf[26]);
    tmp.battery_pct = buf[28];
    tmp.gps_fix_type = buf[29];
    tmp.gps_sat_count = buf[30];
    tmp.flight_mode = buf[31];
    tmp.status_flags = buf[32];

    if (tmp.flight_mode > UAVLINK_MODE_LANDED)   return UAVLINK_ERR_ENUM;

    if (tmp.gps_fix_type > UAVLINK_GPS_FIX_RTK)  return UAVLINK_ERR_ENUM;

    *tm = tmp;
    return UAVLINK_OK;

}

uavlink_result_t uavlink_encode_command(const uavlink_command_t *cmd, uint8_t *buf, size_t buf_len) {

    if (buf == NULL || cmd == NULL) {
        return UAVLINK_ERR_NULL;
    }

    if (buf_len < UAVLINK_COMMAND_PAYLOAD_SIZE) {
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }

    if (cmd->cmd_type > UAVLINK_CMD_RTL) {
        return UAVLINK_ERR_ENUM;
    }

    put_u32(&buf[0], cmd->session_id);
    buf[4] = cmd->cmd_type;
    put_u32(&buf[5], (uint32_t)cmd->param1);
    put_u32(&buf[9], (uint32_t)cmd->param2);
    put_u32(&buf[13], (uint32_t)cmd->param3);

    return UAVLINK_OK;

}

uavlink_result_t uavlink_decode_command(const uint8_t *buf, size_t buf_len, uavlink_command_t *cmd) {

    if (buf == NULL || cmd == NULL) {
        return UAVLINK_ERR_NULL;
    }

    if (buf_len < UAVLINK_COMMAND_PAYLOAD_SIZE) {
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }

    uavlink_command_t tmp;
    tmp.session_id = get_u32(&buf[0]);
    tmp.cmd_type = buf[4];
    tmp.param1 = (int32_t)get_u32(&buf[5]);
    tmp.param2 = (int32_t)get_u32(&buf[9]);
    tmp.param3 = (int32_t)get_u32(&buf[13]);

    if (tmp.cmd_type > UAVLINK_CMD_RTL) {
        return UAVLINK_ERR_ENUM;
    }

    *cmd = tmp;
    return UAVLINK_OK;

}

uavlink_result_t uavlink_encode_ack(const uavlink_ack_t *ack, uint8_t *buf, size_t buf_len) {

    if (buf == NULL || ack == NULL) {
        return UAVLINK_ERR_NULL;
    }

    if (buf_len < UAVLINK_ACK_PAYLOAD_SIZE) {
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }

    if (ack->reason_code > UAVLINK_ACK_INVALID_PARAM) {
        return UAVLINK_ERR_ENUM;
    }

    put_u32(&buf[0], ack->session_id);
    put_u32(&buf[4], ack->ack_seq);
    buf[8] = ack->reason_code;

    return UAVLINK_OK;

}

uavlink_result_t uavlink_decode_ack(const uint8_t *buf, size_t buf_len, uavlink_ack_t *ack) {

    if (buf == NULL || ack == NULL) {
        return UAVLINK_ERR_NULL;
    }

    if (buf_len < UAVLINK_ACK_PAYLOAD_SIZE) {
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }

    uavlink_ack_t tmp;
    tmp.session_id = get_u32(&buf[0]);
    tmp.ack_seq = get_u32(&buf[4]);
    tmp.reason_code = buf[8];

    if (tmp.reason_code > UAVLINK_ACK_INVALID_PARAM) {
        return UAVLINK_ERR_ENUM;
    }

    *ack = tmp;
    return UAVLINK_OK;

}

uavlink_result_t uavlink_encode_packet(const uavlink_packet_t *pkt, uint8_t *buf, size_t buf_len, size_t *packet_len){

    if (buf == NULL || pkt == NULL || packet_len == NULL) {
        return UAVLINK_ERR_NULL;
    }

    int32_t payload_size_res = get_payload_size(pkt->header.msg_type);
    if (payload_size_res < 0) {
        return UAVLINK_ERR_MSG_TYPE;
    }

    size_t payload_size = (size_t)payload_size_res;

    size_t total_len = UAVLINK_HEADER_SIZE + payload_size + UAVLINK_CRC_SIZE;
    if (buf_len < total_len){
        return UAVLINK_ERR_BUFFER_TOO_SMALL;
    }


    uavlink_header_t hdr;
    hdr.version = UAVLINK_VERSION;
    hdr.msg_type = pkt->header.msg_type;
    hdr.seq = pkt->header.seq;
    hdr.payload_len = (uint8_t)payload_size;

    uavlink_result_t hdr_res = uavlink_encode_header(&hdr, buf, buf_len);
    if (hdr_res != UAVLINK_OK) {
        return hdr_res;
    }

    uavlink_result_t payload_res = UAVLINK_OK;

    switch (pkt->header.msg_type) {
        case UAVLINK_MSG_TELEMETRY:
            payload_res = uavlink_encode_telemetry(&pkt->payload.telemetry, buf + UAVLINK_HEADER_SIZE, payload_size);
            break;
        
        case UAVLINK_MSG_COMMAND:
            payload_res = uavlink_encode_command(&pkt->payload.command, buf + UAVLINK_HEADER_SIZE, payload_size);
            break;

        case UAVLINK_MSG_HEARTBEAT:
            break;
        
        case UAVLINK_MSG_ACK:
        case UAVLINK_MSG_NACK:
            payload_res = uavlink_encode_ack(&pkt->payload.ack, buf + UAVLINK_HEADER_SIZE, payload_size);
            break;
        
        default:
            return UAVLINK_ERR_MSG_TYPE;
    }

    if (payload_res != UAVLINK_OK) {
        return payload_res;
    }

    uint16_t crc_val = uavlink_crc16(buf, UAVLINK_HEADER_SIZE + payload_size);
    put_u16(&buf[UAVLINK_HEADER_SIZE + payload_size], crc_val);

    *packet_len = total_len;
    return UAVLINK_OK;

}

uavlink_result_t uavlink_decode_packet(const uint8_t *buf, size_t buf_len, uavlink_packet_t *pkt) {

    if (buf == NULL || pkt == NULL) {
        return UAVLINK_ERR_NULL;
    }

    uavlink_packet_t tmp;

    uavlink_result_t hdr_res = uavlink_decode_header(buf, buf_len, &tmp.header);
    if (hdr_res != UAVLINK_OK) {
        return hdr_res;
    }

    int32_t payload_size = get_payload_size(tmp.header.msg_type);
    if (payload_size < 0) {
        return UAVLINK_ERR_MSG_TYPE;
    }

    size_t payload_len = (size_t)payload_size;

    if (payload_len != tmp.header.payload_len) {
        return UAVLINK_ERR_LENGTH;
    }

    uint16_t crc_val = uavlink_crc16(buf, UAVLINK_HEADER_SIZE + payload_len);
    uint16_t crc_val_check = get_u16(&buf[UAVLINK_HEADER_SIZE + payload_len]);

    if (crc_val != crc_val_check) {

        return UAVLINK_ERR_CRC;
    }

    uavlink_result_t payload_res = UAVLINK_OK;

    switch (tmp.header.msg_type) {
        case UAVLINK_MSG_TELEMETRY:
            payload_res = uavlink_decode_telemetry(buf + UAVLINK_HEADER_SIZE, payload_len, &tmp.payload.telemetry);
            break;
        
        case UAVLINK_MSG_COMMAND:
            payload_res = uavlink_decode_command(buf + UAVLINK_HEADER_SIZE, payload_len, &tmp.payload.command);
            break;

        case UAVLINK_MSG_HEARTBEAT:
            break;
        
        case UAVLINK_MSG_ACK:
        case UAVLINK_MSG_NACK:
            payload_res = uavlink_decode_ack(buf + UAVLINK_HEADER_SIZE, payload_len, &tmp.payload.ack);
            break;
        
        default:
            return UAVLINK_ERR_MSG_TYPE;
    }

    if (payload_res != UAVLINK_OK) {
        return payload_res;
    }

    *pkt = tmp;
    return UAVLINK_OK;
}

