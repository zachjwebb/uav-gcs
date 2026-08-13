#include <uavlink/packet.h>

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
        default:                           return "UNKNOWN_UAVLINK_ERROR";
    }
}