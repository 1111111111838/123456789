/*
 * serial.h - Real Windows COM-port transport for the Unisoc download link.
 *
 * The Spreadtrum download driver exposes the device as a virtual COM port.
 * This transport opens that port with a large buffer, 8N1 framing and the
 * requested baud, and performs timed reads/writes. It is the ONLY transport
 * that touches real hardware; everything about a real dump flows through here.
 */
#ifndef UDT_SERIAL_H
#define UDT_SERIAL_H

#include <stdint.h>

#include "transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Open COM port 'port' (e.g. "COM7") at 'baud'. Returns a heap-allocated
 * transport in *out that must be released with t->close(t). Returns a negative
 * udt_status on failure.
 */
udt_status udt_serial_open(const char *port, uint32_t baud, udt_transport **out);

#ifdef __cplusplus
}
#endif

#endif /* UDT_SERIAL_H */
