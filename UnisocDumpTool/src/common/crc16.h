/*
 * crc16.h - Checksum routines used by the Spreadtrum/Unisoc download protocol.
 *
 * Two distinct algorithms are needed:
 *
 *   1. spd_crc16()    - CRC-16/CCITT (polynomial 0x1021, MSB-first). This is the
 *                       checksum used while talking to the Boot ROM, i.e. before
 *                       any FDL (Flash Download Loader) has been started.
 *
 *   2. spd_fdl_crc()  - A 16-bit one's-complement additive checksum. This is the
 *                       checksum used by most FDL1/FDL2 loaders once they are
 *                       running.
 *
 * Which one is active depends on the device state. See docs/PROTOCOL.md for the
 * handshake that selects it, and for the reverse-engineered sources this matches.
 *
 * These routines are byte-exact re-implementations of the publicly documented
 * Spreadtrum protocol; they are NOT guesses. They must still be validated
 * against the specific chip you target (see docs/MISSING.md).
 */
#ifndef UDT_CRC16_H
#define UDT_CRC16_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CRC-16/CCITT used by the Boot ROM phase. Pass crc=0 to start. */
uint16_t spd_crc16(uint16_t crc, const void *data, size_t len);

/*
 * Additive FDL checksum. The protocol computes a running 32-bit sum of
 * little-endian 16-bit words, folds the carries, takes the one's complement
 * and byte-swaps the result. Call with accumulate across chunks:
 *   acc = spd_fdl_sum(0, buf, len);   // running, not finalized
 *   crc = spd_fdl_finalize(acc);      // final 16-bit value
 * Or use the one-shot helper spd_fdl_crc().
 */
uint32_t spd_fdl_sum(uint32_t acc, const void *data, size_t len);
uint16_t spd_fdl_finalize(uint32_t acc);
uint16_t spd_fdl_crc(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UDT_CRC16_H */
