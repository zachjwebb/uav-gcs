#include <uavlink/crc.h>

/* Computes CRC-16-CCITT over data[0..len-1].
 * Poly 0x1021, init 0xFFFF, no final XOR, no reflection.
 * Precondition: data must be non-NULL when len > 0. */
uint16_t uavlink_crc16(const uint8_t *data, size_t len){
    
    uint16_t crc = 0xFFFF;

    for(size_t i = 0; i < len; i++){
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for(unsigned j = 0; j < 8; j++){
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc =(uint16_t)(crc << 1);
            }
        }
    }

    return crc;

}