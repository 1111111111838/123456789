#include "crc16.h"

uint16_t spd_crc16(uint16_t crc, const void *data, size_t len)
{
    const uint8_t *s = (const uint8_t *)data;
    while (len--) {
        int i;
        crc ^= (uint16_t)(*s++) << 8;
        for (i = 0; i < 8; i++) {
            /* Branch-free: subtract the top bit, mask, XOR with the poly.
             * poly 0x1021, MSB-first. */
            crc = (uint16_t)((crc << 1) ^ ((uint16_t)(0 - (crc >> 15)) & 0x1021));
        }
    }
    return crc;
}

uint32_t spd_fdl_sum(uint32_t acc, const void *data, size_t len)
{
    const uint8_t *s = (const uint8_t *)data;
    while (len > 1) {
        acc += (uint32_t)s[1] << 8 | s[0];
        s += 2;
        len -= 2;
    }
    if (len) {
        acc += *s;
    }
    return acc;
}

uint16_t spd_fdl_finalize(uint32_t acc)
{
    acc = (acc >> 16) + (acc & 0xffff);
    acc += acc >> 16;
    acc = ~acc & 0xffff;
    /* byte swap */
    return (uint16_t)((acc >> 8) | ((acc & 0xff) << 8));
}

uint16_t spd_fdl_crc(const void *data, size_t len)
{
    return spd_fdl_finalize(spd_fdl_sum(0, data, len));
}
