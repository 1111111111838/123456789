#include "transport.h"

#include "../common/logging.h"

const char *udt_status_str(udt_status s)
{
    switch (s) {
    case UDT_OK:             return "OK";
    case UDT_ERR_TIMEOUT:    return "timeout";
    case UDT_ERR_IO:         return "I/O error";
    case UDT_ERR_CLOSED:     return "transport closed";
    case UDT_ERR_PROTOCOL:   return "protocol error";
    case UDT_ERR_CRC:        return "checksum mismatch";
    case UDT_ERR_NACK:       return "device reported failure";
    case UDT_ERR_CANCELLED:  return "cancelled";
    case UDT_ERR_ARG:        return "invalid argument";
    case UDT_ERR_NOMEM:      return "out of memory";
    case UDT_ERR_UNSUPPORTED:return "unsupported operation";
    case UDT_ERR_NODEVICE:   return "no device";
    default:                 return "unknown error";
    }
}

udt_status udt_write_all(udt_transport *t, const uint8_t *buf, size_t len)
{
    int n;
    if (!t || !t->write) return UDT_ERR_ARG;
    n = t->write(t, buf, len);
    if (n < 0) return (udt_status)n;
    if ((size_t)n != len) return UDT_ERR_IO;
    return UDT_OK;
}

udt_status udt_read_exact(udt_transport *t, uint8_t *buf, size_t len, int timeout_ms)
{
    size_t got = 0;
    if (!t || !t->read) return UDT_ERR_ARG;
    while (got < len) {
        int n = t->read(t, buf + got, len - got, timeout_ms);
        if (n < 0) return (udt_status)n;
        if (n == 0) return UDT_ERR_TIMEOUT;
        got += (size_t)n;
    }
    return UDT_OK;
}
