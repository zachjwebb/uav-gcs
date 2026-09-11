#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uavlink/packet.h>

#define ITERATIONS 100000

int main(void) {
    srand(12345);   /* fixed seed: reproducible */

    uint8_t buf[UAVLINK_MAX_PACKET + 16];
    uavlink_packet_t pkt;
    size_t ok = 0;

    for (size_t i = 0; i < ITERATIONS; i++) {
        size_t len = (size_t)(rand() % (int)sizeof(buf));
        for (size_t j = 0; j < len; j++) {
            buf[j] = (uint8_t)(rand() & 0xFF);
        }
        if (uavlink_decode_packet(buf, len, &pkt) == UAVLINK_OK) {
            ok++;
        }
    }

    printf("fuzz: %d iterations, %zu accepted\n", ITERATIONS, ok);
    return 0;
}