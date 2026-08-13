#ifndef UAVLINK_CRC_H
#define UAVLINK_CRC_H

#include <stdint.h>
#include <stddef.h>

uint16_t uavlink_crc16(const uint8_t *data, size_t len);

#endif