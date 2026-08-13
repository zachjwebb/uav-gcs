#include <stdio.h>
#include <uavlink/crc.h>

int main(void) {
    const uint8_t check[] = "123456789";
    uint16_t got = uavlink_crc16(check, 9);

    if (got != 0x29B1) {
        fprintf(stderr, "FAIL: expected 0x29B1, got 0x%04X\n", got);
        return 1;
    }
    printf("PASS: crc16 check value correct\n");
    return 0;
}