/*
 * transport.h - Abstract byte transport.
 *
 * The SPRD protocol layer reads and writes raw bytes through this interface and
 * never learns whether the bytes travel over a real USB/serial link or a
 * simulated device. This is the single seam that keeps "simulation mode" and
 * "real mode" completely separate, as required by the specification: a real
 * dump is one produced with a real transport, period.
 *
 * Implementations:
 *   comm/serial.c  -> udt_serial_open()    (real Windows COM / download port)
 *   sim/sim_device.c -> udt_sim_open()     (in-process fake, never on hardware)
 */
#ifndef UDT_TRANSPORT_H
#define UDT_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UDT_OK = 0,
    UDT_ERR_TIMEOUT    = -1,
    UDT_ERR_IO         = -2,
    UDT_ERR_CLOSED     = -3,
    UDT_ERR_PROTOCOL   = -4,
    UDT_ERR_CRC        = -5,
    UDT_ERR_NACK       = -6,
    UDT_ERR_CANCELLED  = -7,
    UDT_ERR_ARG        = -8,
    UDT_ERR_NOMEM      = -9,
    UDT_ERR_UNSUPPORTED= -10,
    UDT_ERR_NODEVICE   = -11
} udt_status;

const char *udt_status_str(udt_status s);

typedef struct udt_transport udt_transport;

struct udt_transport {
    void *ctx;

    /* Returns number of bytes written (>=0) or a negative udt_status on error.
     * Must write all of len or return an error. */
    int (*write)(udt_transport *t, const uint8_t *buf, size_t len);

    /* Reads up to len bytes, blocking until at least one byte arrives or
     * timeout_ms elapses. Returns bytes read (0 on timeout) or negative
     * udt_status. */
    int (*read)(udt_transport *t, uint8_t *buf, size_t len, int timeout_ms);

    /* Drop any buffered input/output. */
    void (*flush)(udt_transport *t);

    /* Change line speed if the transport supports it (serial). Simulated and
     * fixed-rate transports may return UDT_ERR_UNSUPPORTED. */
    udt_status (*set_baud)(udt_transport *t, uint32_t baud);

    /* Human-readable name, e.g. "COM7" or "SIM:SC6531E". */
    const char *(*name)(udt_transport *t);

    void (*close)(udt_transport *t);
};

/* Convenience wrappers that never partially write. */
udt_status udt_write_all(udt_transport *t, const uint8_t *buf, size_t len);

/* Read exactly len bytes or fail (UDT_ERR_TIMEOUT if the deadline passes
 * before all bytes arrive). */
udt_status udt_read_exact(udt_transport *t, uint8_t *buf, size_t len, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* UDT_TRANSPORT_H */
