#ifndef UAVLINK_PACKET_H
#define UAVLINK_PACKET_H

#include <stdint.h>
#include <stddef.h>

// Common UAVLink packet definitions
#define UAVLINK_VERSION     1
#define UAVLINK_HEADER_SIZE 8
#define UAVLINK_CRC_SIZE    2
#define UAVLINK_MAX_PACKET  256
#define UAVLINK_MAX_PAYLOAD (UAVLINK_MAX_PACKET - UAVLINK_HEADER_SIZE - UAVLINK_CRC_SIZE)

//Telemetry, Heartbeat, Command, ACK/NACK Definitions
#define UAVLINK_TELEMETRY_PAYLOAD_SIZE 33
#define UAVLINK_HEARTBEAT_PAYLOAD_SIZE 0
#define UAVLINK_COMMAND_PAYLOAD_SIZE 17
#define UAVLINK_ACK_PAYLOAD_SIZE 9

#define UAVLINK_STATUS_ARMED  0x01

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
    UAVLINK_ERR_ENUM             /* invalid enum value */
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

typedef enum {
    UAVLINK_GPS_FIX_NONE = 0,
    UAVLINK_GPS_FIX_2D   = 1,
    UAVLINK_GPS_FIX_3D   = 2,
    UAVLINK_GPS_FIX_RTK  = 3
} uavlink_gps_fix_t;

typedef enum {
    UAVLINK_MODE_BOOT      = 0,
    UAVLINK_MODE_PREFLIGHT = 1,
    UAVLINK_MODE_ARMED     = 2,
    UAVLINK_MODE_AUTO      = 3,
    UAVLINK_MODE_RTL       = 4,
    UAVLINK_MODE_LANDING   = 5,
    UAVLINK_MODE_LANDED    = 6
} uavlink_flight_mode_t;

typedef struct {
    uint32_t timestamp_ms;
    int32_t latitude;  /* degrees * 1e7 */
    int32_t longitude; /* degrees * 1e7 */
    int32_t altitude_amsl; /* meters x 100 */
    uint16_t ground_speed;    /* m/s × 100 */
    int16_t  vertical_speed;  /* m/s × 100, positive = climbing */
    uint16_t heading; /* degrees × 100, 0–35999 */
    int16_t roll; /* degrees × 100, positive = right wing down */
    int16_t pitch; /* degrees × 100, positive = nose up */
    uint16_t battery_voltage; /* millivolts */
    uint8_t battery_pct;
    uint8_t gps_fix_type;
    uint8_t gps_sat_count;
    uint8_t flight_mode;
    uint8_t status_flags;
} uavlink_telemetry_t;

/* Encodes hdr into buf as UAVLINK_HEADER_SIZE big-endian bytes.
 * Reserved byte 7 is written as 0x00.
 * Returns UAVLINK_ERR_BUFFER_TOO_SMALL if buf_len < UAVLINK_HEADER_SIZE. */
uavlink_result_t uavlink_encode_header(const uavlink_header_t *hdr, uint8_t *buf, size_t buf_len);

/* Decodes and validates a header. buf_len must be the full packet length
 * as received, not just the header size — the length consistency check
 * requires it. Byte 7 (reserved) is ignored. */
uavlink_result_t uavlink_decode_header(const uint8_t *buf, size_t buf_len, uavlink_header_t *hdr);

const char *uavlink_strerror(uavlink_result_t r);

uavlink_result_t uavlink_encode_telemetry(const uavlink_telemetry_t *tm, uint8_t *buf, size_t buf_len);

uavlink_result_t uavlink_decode_telemetry(const uint8_t *buf, size_t buf_len, uavlink_telemetry_t *tm);

#endif 