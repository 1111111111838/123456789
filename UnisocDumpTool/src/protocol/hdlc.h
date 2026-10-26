/*
 * hdlc.h - HDLC-style framing used by the Spreadtrum/Unisoc download protocol.
 *
 * Every protocol message is wrapped as:
 *
 *     0x7E  <escaped message bytes>  0x7E
 *
 * Inside the frame two bytes are escaped so they can never be confused with the
 * 0x7E delimiter:
 *
 *     0x7E -> 0x7D 0x5E
 *     0x7D -> 0x7D 0x5D
 *
 * The "message bytes" that get escaped are: type(2) + length(2) + data + crc(2),
 * all assembled by the sprd_proto layer. This module only does the framing.
 *
 * Reference: publicly reverse-engineered Spreadtrum protocol (see docs/PROTOCOL.md).
 */
#ifndef UDT_HDLC_H
#define UDT_HDLC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HDLC_FLAG   0x7E
#define HDLC_ESC    0x7D
#define HDLC_ESC_XOR 0x20   /* escaped byte is (orig ^ 0x20) after 0x7D */

/*
 * Encode one complete frame around 'payload'. Writes a leading 0x7E, the
 * escaped payload, and a trailing 0x7E. Returns the number of bytes written,
 * or 0 if 'out' is too small (worst case: 2*len + 2).
 */
size_t hdlc_encode(const uint8_t *payload, size_t len, uint8_t *out, size_t outcap);

/*
 * Un-escape a frame body (the bytes strictly between the two 0x7E delimiters).
 * Returns the decoded length, or (size_t)-1 on a malformed escape sequence or
 * if 'out' is too small.
 */
size_t hdlc_decode(const uint8_t *in, size_t len, uint8_t *out, size_t outcap);

#ifdef __cplusplus
}
#endif

#endif /* UDT_HDLC_H */
