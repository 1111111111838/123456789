#include "serial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/logging.h"

#ifdef _WIN32

#include <windows.h>

typedef struct {
    HANDLE   h;
    char     name[32];
    OVERLAPPED ov_read;
    OVERLAPPED ov_write;
} serial_ctx;

static int serial_write(udt_transport *t, const uint8_t *buf, size_t len)
{
    serial_ctx *c = (serial_ctx *)t->ctx;
    DWORD written = 0;
    size_t total = 0;

    while (total < len) {
        DWORD chunk = (DWORD)((len - total > 0x10000) ? 0x10000 : (len - total));
        ResetEvent(c->ov_write.hEvent);
        if (!WriteFile(c->h, buf + total, chunk, &written, &c->ov_write)) {
            if (GetLastError() != ERROR_IO_PENDING) return UDT_ERR_IO;
            if (WaitForSingleObject(c->ov_write.hEvent, INFINITE) != WAIT_OBJECT_0)
                return UDT_ERR_IO;
            if (!GetOverlappedResult(c->h, &c->ov_write, &written, FALSE))
                return UDT_ERR_IO;
        }
        if (written == 0) return UDT_ERR_IO;
        total += written;
    }
    return (int)total;
}

static int serial_read(udt_transport *t, uint8_t *buf, size_t len, int timeout_ms)
{
    serial_ctx *c = (serial_ctx *)t->ctx;
    DWORD got = 0;

    ResetEvent(c->ov_read.hEvent);
    if (!ReadFile(c->h, buf, (DWORD)len, &got, &c->ov_read)) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) return UDT_ERR_IO;
        {
            DWORD w = WaitForSingleObject(c->ov_read.hEvent,
                                          timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);
            if (w == WAIT_TIMEOUT) {
                CancelIo(c->h);
                /* collect whatever completed */
                GetOverlappedResult(c->h, &c->ov_read, &got, TRUE);
                return (int)got;   /* may be 0 == timeout */
            }
            if (w != WAIT_OBJECT_0) return UDT_ERR_IO;
            if (!GetOverlappedResult(c->h, &c->ov_read, &got, FALSE))
                return UDT_ERR_IO;
        }
    }
    return (int)got;
}

static void serial_flush(udt_transport *t)
{
    serial_ctx *c = (serial_ctx *)t->ctx;
    PurgeComm(c->h, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
}

static udt_status serial_set_baud(udt_transport *t, uint32_t baud)
{
    serial_ctx *c = (serial_ctx *)t->ctx;
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(c->h, &dcb)) return UDT_ERR_IO;
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    dcb.fParity  = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl  = DTR_CONTROL_ENABLE;
    dcb.fRtsControl  = RTS_CONTROL_ENABLE;
    if (!SetCommState(c->h, &dcb)) return UDT_ERR_IO;
    return UDT_OK;
}

static const char *serial_name(udt_transport *t)
{
    return ((serial_ctx *)t->ctx)->name;
}

static void serial_close(udt_transport *t)
{
    if (t) {
        serial_ctx *c = (serial_ctx *)t->ctx;
        if (c) {
            if (c->ov_read.hEvent)  CloseHandle(c->ov_read.hEvent);
            if (c->ov_write.hEvent) CloseHandle(c->ov_write.hEvent);
            if (c->h && c->h != INVALID_HANDLE_VALUE) CloseHandle(c->h);
            free(c);
        }
        free(t);
    }
}

udt_status udt_serial_open(const char *port, uint32_t baud, udt_transport **out)
{
    char path[32];
    serial_ctx *c;
    udt_transport *t;
    COMMTIMEOUTS to;

    if (!port || !out) return UDT_ERR_ARG;
    *out = NULL;

    c = (serial_ctx *)calloc(1, sizeof(*c));
    t = (udt_transport *)calloc(1, sizeof(*t));
    if (!c || !t) { free(c); free(t); return UDT_ERR_NOMEM; }

    snprintf(c->name, sizeof(c->name), "%s", port);
    /* \\.\COMn form works for COM10+ as well. */
    snprintf(path, sizeof(path), "\\\\.\\%s", port);

    c->h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                       OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (c->h == INVALID_HANDLE_VALUE) {
        LOGE("Cannot open %s (error %lu)", port, (unsigned long)GetLastError());
        free(c); free(t);
        return UDT_ERR_IO;
    }

    SetupComm(c->h, 1 << 20, 1 << 20);   /* generous driver buffers */

    c->ov_read.hEvent  = CreateEventA(NULL, TRUE, FALSE, NULL);
    c->ov_write.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!c->ov_read.hEvent || !c->ov_write.hEvent) {
        serial_close(t);  /* t->ctx not set yet */
        return UDT_ERR_IO;
    }

    /* Return as soon as any byte is available; our own timeouts govern waits. */
    memset(&to, 0, sizeof(to));
    to.ReadIntervalTimeout = MAXDWORD;
    to.ReadTotalTimeoutConstant = 0;
    to.ReadTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 2000;
    to.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(c->h, &to);

    t->ctx = c;
    t->write = serial_write;
    t->read = serial_read;
    t->flush = serial_flush;
    t->set_baud = serial_set_baud;
    t->name = serial_name;
    t->close = serial_close;

    {
        udt_status st = serial_set_baud(t, baud ? baud : 115200);
        if (st != UDT_OK) { serial_close(t); return st; }
    }
    serial_flush(t);

    LOGI("Opened %s at %u baud", port, (unsigned)(baud ? baud : 115200));
    *out = t;
    return UDT_OK;
}

#else /* non-Windows: no real serial */

udt_status udt_serial_open(const char *port, uint32_t baud, udt_transport **out)
{
    (void)port; (void)baud;
    if (out) *out = NULL;
    LOGE("Real serial transport is Windows-only in this build");
    return UDT_ERR_UNSUPPORTED;
}

#endif
