#include "sprd_proto.h"

#include <string.h>

#include "hdlc.h"
#include "../common/crc16.h"
#include "../common/logging.h"

#define DEFAULT_TIMEOUT_MS 3000

void sprd_put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

uint32_t sprd_get_be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

void sprd_session_init(sprd_session *s, udt_transport *t)
{
    memset(s, 0, sizeof(*s));
    s->t = t;
    s->use_fdl_crc = 0;        /* Boot ROM phase first */
    s->transcode = 1;
    s->default_timeout_ms = DEFAULT_TIMEOUT_MS;
}

/*
 * Assemble a raw (un-escaped) protocol message: type(2 BE) length(2 BE)
 * data[length] crc(2 BE), where crc covers type+length+data. The checksum
 * algorithm depends on the session phase.
 */
static size_t build_message(const sprd_session *s, uint16_t type,
                            const uint8_t *data, uint16_t len,
                            uint8_t *out, size_t outcap)
{
    size_t n = 4 + (size_t)len + 2;
    uint16_t crc;

    if (n > outcap) return 0;

    out[0] = (uint8_t)(type >> 8);
    out[1] = (uint8_t)(type);
    out[2] = (uint8_t)(len >> 8);
    out[3] = (uint8_t)(len);
    if (len && data) {
        memcpy(out + 4, data, len);
    }

    if (s->use_fdl_crc) {
        crc = spd_fdl_crc(out, 4 + (size_t)len);
    } else {
        crc = spd_crc16(0, out, 4 + (size_t)len);
    }
    out[4 + len]     = (uint8_t)(crc >> 8);
    out[4 + len + 1] = (uint8_t)(crc);
    return n;
}

udt_status sprd_send(sprd_session *s, uint16_t type, const uint8_t *data, uint16_t len)
{
    uint8_t raw[8 + SPRD_MAX_DATA];
    uint8_t frame[2 * (8 + SPRD_MAX_DATA) + 2];
    size_t rawlen, framelen;

    if (len > SPRD_MAX_DATA) return UDT_ERR_ARG;

    rawlen = build_message(s, type, data, len, raw, sizeof(raw));
    if (rawlen == 0) return UDT_ERR_NOMEM;

    if (s->transcode) {
        framelen = hdlc_encode(raw, rawlen, frame, sizeof(frame));
        if (framelen == 0) return UDT_ERR_NOMEM;
        return udt_write_all(s->t, frame, framelen);
    } else {
        /* Transcoding disabled: still delimited by 0x7E but no escaping. */
        uint8_t flag = HDLC_FLAG;
        udt_status st;
        if ((st = udt_write_all(s->t, &flag, 1)) != UDT_OK) return st;
        if ((st = udt_write_all(s->t, raw, rawlen)) != UDT_OK) return st;
        return udt_write_all(s->t, &flag, 1);
    }
}

/* Read one HDLC frame body (bytes between two 0x7E) into buf. */
static udt_status read_frame_raw(sprd_session *s, uint8_t *buf, size_t cap,
                                 size_t *out_len, int timeout_ms)
{
    size_t n = 0;
    int started = 0;
    uint8_t b;

    for (;;) {
        int r = s->t->read(s->t, &b, 1, timeout_ms);
        if (r < 0) return (udt_status)r;
        if (r == 0) return UDT_ERR_TIMEOUT;

        if (b == HDLC_FLAG) {
            if (!started) {
                started = 1;           /* opening flag */
                continue;
            }
            if (n == 0) {
                /* Two flags back to back: treat as still opening. */
                continue;
            }
            *out_len = n;              /* closing flag */
            return UDT_OK;
        }
        if (!started) {
            /* Garbage before a frame; ignore until we see an opening flag. */
            continue;
        }
        if (n >= cap) return UDT_ERR_PROTOCOL;
        buf[n++] = b;
    }
}

udt_status sprd_recv(sprd_session *s, sprd_msg *msg, int timeout_ms)
{
    uint8_t framebuf[2 * (8 + SPRD_MAX_DATA) + 2];
    uint8_t raw[8 + SPRD_MAX_DATA];
    size_t framelen = 0, rawlen;
    uint16_t len, crc_got, crc_calc;

    udt_status st = read_frame_raw(s, framebuf, sizeof(framebuf), &framelen, timeout_ms);
    if (st != UDT_OK) return st;

    if (s->transcode) {
        rawlen = hdlc_decode(framebuf, framelen, raw, sizeof(raw));
        if (rawlen == (size_t)-1) return UDT_ERR_PROTOCOL;
    } else {
        if (framelen > sizeof(raw)) return UDT_ERR_PROTOCOL;
        memcpy(raw, framebuf, framelen);
        rawlen = framelen;
    }

    if (rawlen < 6) return UDT_ERR_PROTOCOL;   /* type+len+crc minimum */

    msg->type = (uint16_t)(raw[0] << 8 | raw[1]);
    len = (uint16_t)(raw[2] << 8 | raw[3]);
    if ((size_t)len + 6 > rawlen) return UDT_ERR_PROTOCOL;
    if (len > SPRD_MAX_DATA) return UDT_ERR_PROTOCOL;

    crc_got = (uint16_t)(raw[4 + len] << 8 | raw[4 + len + 1]);
    if (s->use_fdl_crc) {
        crc_calc = spd_fdl_crc(raw, 4 + (size_t)len);
    } else {
        crc_calc = spd_crc16(0, raw, 4 + (size_t)len);
    }
    if (crc_got != crc_calc) {
        LOGW("frame checksum mismatch: got %04X want %04X (type %04X)",
             crc_got, crc_calc, msg->type);
        return UDT_ERR_CRC;
    }

    msg->length = len;
    if (len) memcpy(msg->data, raw + 4, len);
    return UDT_OK;
}

udt_status sprd_transact(sprd_session *s, uint16_t cmd,
                         const uint8_t *data, uint16_t len,
                         sprd_msg *reply, int timeout_ms)
{
    udt_status st = sprd_send(s, cmd, data, len);
    if (st != UDT_OK) return st;
    return sprd_recv(s, reply, timeout_ms > 0 ? timeout_ms : s->default_timeout_ms);
}

udt_status sprd_command_ack(sprd_session *s, uint16_t cmd,
                            const uint8_t *data, uint16_t len, int timeout_ms)
{
    sprd_msg reply;
    udt_status st = sprd_transact(s, cmd, data, len, &reply, timeout_ms);
    if (st != UDT_OK) return st;
    if (reply.type == BSL_REP_ACK) return UDT_OK;
    LOGW("command %04X: device replied %04X (not ACK)", cmd, reply.type);
    return UDT_ERR_NACK;
}

udt_status sprd_handshake(sprd_session *s, char *ver_out, size_t ver_cap)
{
    /*
     * Boot ROM handshake. The device waits for a stream of HDLC flag bytes;
     * after it synchronises it answers the first well-formed CONNECT (or in
     * many ROMs, the bare flags) with a BSL_REP_VER frame carrying its version
     * string. We send a short run of flags, then a CONNECT, and read the reply.
     */
    uint8_t hello[64];
    sprd_msg reply;
    udt_status st;
    int attempt;

    s->use_fdl_crc = 0;
    s->transcode = 1;

    memset(hello, HDLC_FLAG, sizeof(hello));

    for (attempt = 0; attempt < 5; attempt++) {
        st = udt_write_all(s->t, hello, sizeof(hello));
        if (st != UDT_OK) return st;

        st = sprd_recv(s, &reply, 500);
        if (st == UDT_OK && reply.type == BSL_REP_VER) {
            goto got_ver;
        }

        /* Some ROMs need an explicit CONNECT after sync. */
        st = sprd_transact(s, BSL_CMD_CONNECT, NULL, 0, &reply, 500);
        if (st == UDT_OK &&
            (reply.type == BSL_REP_VER || reply.type == BSL_REP_ACK)) {
            goto got_ver;
        }
        LOGD("handshake attempt %d: no version yet (%s)", attempt + 1,
             udt_status_str(st));
    }
    LOGE("Boot ROM did not answer the handshake");
    return UDT_ERR_NODEVICE;

got_ver:
    if (ver_out && ver_cap) {
        size_t n = reply.length < ver_cap - 1 ? reply.length : ver_cap - 1;
        memcpy(ver_out, reply.data, n);
        ver_out[n] = '\0';
    }
    LOGI("Boot ROM handshake OK (reply type %04X, %u bytes)",
         reply.type, reply.length);
    return UDT_OK;
}
