#ifndef UAVLINK_PACKET_H
#define UAVLINK_PACKET_H

#include <stdint.h>
#include <stddef.h>

#define UAVLINK_VERSION     1
#define UAVLINK_HEADER_SIZE 8
#define UAVLINK_CRC_SIZE    2
#define UAVLINK_MAX_PACKET  256
#define UAVLINK_MAX_PAYLOAD (UAVLINK_MAX_PACKET - UAVLINK_HEADER_SIZE - UAVLINK_CRC_SIZE)

typedef enum {
    UAVLINK_OK = 0,
    UAVLINK_ERR_NULL,           /* NULL pointer argument */
    UAVLINK_ERR_BUFFER_TOO_SMALL,
    UAVLINK_ERR_VERSION,        /* FMT-001 */
    UAVLINK_ERR_MSG_TYPE,       /* FMT-006 */
    UAVLINK_ERR_LENGTH,         /* FMT-002 */
    UAVLINK_ERR_CRC,            /* FMT-003 */
    UAVLINK_ERR_TRUNCATED,      /* declared length exceeds bytes supplied */
    UAVLINK_ERR_TRAILING_DATA,   /* extra bytes after declared length */
} uavlink_result_t;

typedef enum {
    UAVLINK_MSG_TELEMETRY = 1,
    UAVLINK_MSG_COMMAND   = 2,
    UAVLINK_MSG_HEARTBEAT = 3,
    UAVLINK_MSG_ACK       = 4,
    UAVLINK_MSG_NACK      = 5
} uavlink_msg_type_t;

typedef struct {
    uint8_t  version;
    uint8_t  msg_type;
    uint32_t seq;
    uint8_t  payload_len;
} uavlink_header_t;

/* Encodes hdr into buf as UAVLINK_HEADER_SIZE big-endian bytes.
 * Reserved byte 7 is written as 0x00.
 * Returns UAVLINK_ERR_BUFFER_TOO_SMALL if buf_len < UAVLINK_HEADER_SIZE. */
uavlink_result_t uavlink_encode_header(const uavlink_header_t *hdr, uint8_t *buf, size_t buf_len);

/* Decodes and validates a header. buf_len must be the full packet length
 * as received, not just the header size — the length consistency check
 * requires it. Byte 7 (reserved) is ignored. */
uavlink_result_t uavlink_decode_header(const uint8_t *buf, size_t buf_len, uavlink_header_t *hdr);

const char *uavlink_strerror(uavlink_result_t r);

#endif