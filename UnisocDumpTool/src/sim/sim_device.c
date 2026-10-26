#include "sim_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../protocol/hdlc.h"
#include "../protocol/sprd_proto.h"
#include "../common/crc16.h"
#include "../common/logging.h"

#define SIM_RXCAP (4 * (8 + SPRD_MAX_DATA))
#define SIM_OUTCAP (4 * (8 + SPRD_MAX_DATA))

typedef struct {
    sim_config cfg;
    char name[48];

    /* Bytes the host has written, accumulated until a full frame is seen. */
    uint8_t in[SIM_RXCAP];
    size_t  in_len;

    /* Bytes queued for the host to read (device -> host). */
    uint8_t out[SIM_OUTCAP];
    size_t  out_head, out_tail;

    /* Device state. */
    int handshaked;
    int fdl_running;        /* set after EXEC_DATA */
    int use_fdl_crc;        /* mirror of host's checksum choice */

    /* Active partitioned read. */
    uint32_t read_base;
    uint32_t read_total;
    uint32_t read_offset;
    int      read_active;
} sim_ctx;

uint8_t sim_flash_byte(uint32_t addr)
{
    /* Deterministic, non-trivial pattern so corruption is detectable. */
    uint32_t x = addr * 2654435761u;
    x ^= x >> 15;
    return (uint8_t)(x ^ (addr >> 3));
}

static void out_push(sim_ctx *c, const uint8_t *buf, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        size_t next = (c->out_tail + 1) % SIM_OUTCAP;
        if (next == c->out_head) break;   /* full; drop (test harness reads promptly) */
        c->out[c->out_tail] = buf[i];
        c->out_tail = next;
    }
}

/* Build and enqueue one response frame using the current checksum mode. */
static void sim_reply(sim_ctx *c, uint16_t type, const uint8_t *data, uint16_t len)
{
    uint8_t raw[8 + SPRD_MAX_DATA];
    uint8_t frame[2 * (8 + SPRD_MAX_DATA) + 2];
    size_t rawlen, framelen;
    uint16_t crc;

    raw[0] = (uint8_t)(type >> 8);
    raw[1] = (uint8_t)type;
    raw[2] = (uint8_t)(len >> 8);
    raw[3] = (uint8_t)len;
    if (len) memcpy(raw + 4, data, len);

    if (c->use_fdl_crc) crc = spd_fdl_crc(raw, 4 + (size_t)len);
    else                crc = spd_crc16(0, raw, 4 + (size_t)len);
    raw[4 + len] = (uint8_t)(crc >> 8);
    raw[4 + len + 1] = (uint8_t)crc;
    rawlen = 6 + (size_t)len;

    framelen = hdlc_encode(raw, rawlen, frame, sizeof(frame));
    out_push(c, frame, framelen);
}

/* Validate a raw (decoded) message against either checksum; set mode on match. */
static int sim_check_and_set_crc(sim_ctx *c, const uint8_t *raw, size_t rawlen,
                                 uint16_t *type, uint16_t *len, const uint8_t **data)
{
    uint16_t t, l, got;
    if (rawlen < 6) return 0;
    t = (uint16_t)(raw[0] << 8 | raw[1]);
    l = (uint16_t)(raw[2] << 8 | raw[3]);
    if ((size_t)l + 6 > rawlen) return 0;
    got = (uint16_t)(raw[4 + l] << 8 | raw[4 + l + 1]);

    if (got == spd_crc16(0, raw, 4 + (size_t)l)) {
        c->use_fdl_crc = 0;
    } else if (got == spd_fdl_crc(raw, 4 + (size_t)l)) {
        c->use_fdl_crc = 1;
    } else {
        return 0;   /* bad checksum: device would ignore */
    }
    *type = t; *len = l; *data = raw + 4;
    return 1;
}

static void sim_handle_message(sim_ctx *c, uint16_t type, uint16_t len,
                               const uint8_t *data)
{
    switch (type) {
    case BSL_CMD_CONNECT:
        c->handshaked = 1;
        /* Report version string as a VER reply. */
        sim_reply(c, BSL_REP_VER, (const uint8_t *)c->cfg.chip_name,
                  (uint16_t)strlen(c->cfg.chip_name));
        break;

    case BSL_CMD_START_DATA:
    case BSL_CMD_MIDST_DATA:
    case BSL_CMD_END_DATA:
        sim_reply(c, BSL_REP_ACK, NULL, 0);
        break;

    case BSL_CMD_EXEC_DATA:
        c->fdl_running = 1;
        sim_reply(c, BSL_REP_ACK, NULL, 0);
        break;

    case BSL_CMD_CHANGE_BAUD:
    case BSL_CMD_KEEP_CHARGE:
        sim_reply(c, BSL_REP_ACK, NULL, 0);
        break;

    case BSL_CMD_READ_FLASH: {
        /* Legacy linear read: addr(4), size(4), pad(4). */
        uint32_t addr, size, i;
        uint8_t buf[SPRD_MAX_DATA];
        if (len < 8) { sim_reply(c, BSL_REP_INVALID_CMD, NULL, 0); break; }
        if (c->cfg.require_fdl && !c->fdl_running) {
            sim_reply(c, BSL_REP_OPERATION_FAILED, NULL, 0); break;
        }
        addr = sprd_get_be32(data);
        size = sprd_get_be32(data + 4);
        if (size > sizeof(buf)) size = sizeof(buf);
        for (i = 0; i < size; i++) buf[i] = sim_flash_byte(addr + i);
        sim_reply(c, BSL_REP_READ_FLASH, buf, (uint16_t)size);
        break;
    }

    case BSL_CMD_READ_FLASH_START: {
        /* addr/partition(4), total(4), offset(4). */
        if (len < 12) { sim_reply(c, BSL_REP_INVALID_CMD, NULL, 0); break; }
        if (c->cfg.require_fdl && !c->fdl_running) {
            sim_reply(c, BSL_REP_OPERATION_FAILED, NULL, 0); break;
        }
        c->read_base = sprd_get_be32(data);
        c->read_total = sprd_get_be32(data + 4);
        c->read_offset = sprd_get_be32(data + 8);
        c->read_active = 1;
        sim_reply(c, BSL_REP_ACK, NULL, 0);
        break;
    }

    case BSL_CMD_READ_FLASH_MIDST: {
        /* size(4), offset(4). */
        uint32_t size, offset, i;
        uint8_t buf[SPRD_MAX_DATA];
        if (!c->read_active || len < 8) {
            sim_reply(c, BSL_REP_OPERATION_FAILED, NULL, 0); break;
        }
        size = sprd_get_be32(data);
        offset = sprd_get_be32(data + 4);
        if (size > sizeof(buf)) size = sizeof(buf);
        for (i = 0; i < size; i++) buf[i] = sim_flash_byte(c->read_base + offset + i);
        sim_reply(c, BSL_REP_READ_FLASH, buf, (uint16_t)size);
        break;
    }

    case BSL_CMD_READ_FLASH_END:
        c->read_active = 0;
        sim_reply(c, BSL_REP_ACK, NULL, 0);
        break;

    default:
        sim_reply(c, BSL_REP_UNKNOWN_CMD, NULL, 0);
        break;
    }
}

/* Scan accumulated input for a complete frame; process and drop it. */
static void sim_pump(sim_ctx *c)
{
    for (;;) {
        size_t start = (size_t)-1, end = (size_t)-1, i;
        uint8_t raw[8 + SPRD_MAX_DATA];
        size_t rawlen;
        uint16_t type, len;
        const uint8_t *data;

        /* Find first flag. */
        for (i = 0; i < c->in_len; i++) {
            if (c->in[i] == HDLC_FLAG) { start = i; break; }
        }
        if (start == (size_t)-1) { c->in_len = 0; return; }   /* nothing usable */

        /* Find closing flag after some body. */
        for (i = start + 1; i < c->in_len; i++) {
            if (c->in[i] == HDLC_FLAG) {
                if (i == start + 1) { start = i; continue; }  /* collapse flags */
                end = i; break;
            }
        }
        if (end == (size_t)-1) {
            /* No complete frame yet; compact buffer to the opening flag. */
            if (start > 0) {
                memmove(c->in, c->in + start, c->in_len - start);
                c->in_len -= start;
            }
            return;
        }

        rawlen = hdlc_decode(c->in + start + 1, end - start - 1, raw, sizeof(raw));
        /* Remove consumed bytes (through the closing flag). */
        memmove(c->in, c->in + end + 1, c->in_len - (end + 1));
        c->in_len -= (end + 1);

        if (rawlen == (size_t)-1) continue;   /* malformed; skip */
        if (sim_check_and_set_crc(c, raw, rawlen, &type, &len, &data)) {
            sim_handle_message(c, type, len, data);
        }
    }
}

/* ---- transport vtable ---- */

static int sim_write(udt_transport *t, const uint8_t *buf, size_t len)
{
    sim_ctx *c = (sim_ctx *)t->ctx;
    size_t i;
    for (i = 0; i < len; i++) {
        if (c->in_len < SIM_RXCAP) c->in[c->in_len++] = buf[i];
    }
    sim_pump(c);
    return (int)len;
}

static int sim_read(udt_transport *t, uint8_t *buf, size_t len, int timeout_ms)
{
    sim_ctx *c = (sim_ctx *)t->ctx;
    size_t n = 0;
    (void)timeout_ms;
    while (n < len && c->out_head != c->out_tail) {
        buf[n++] = c->out[c->out_head];
        c->out_head = (c->out_head + 1) % SIM_OUTCAP;
    }
    return (int)n;   /* 0 means "no data" == timeout for the caller */
}

static void sim_flush(udt_transport *t)
{
    sim_ctx *c = (sim_ctx *)t->ctx;
    c->in_len = 0;
    c->out_head = c->out_tail = 0;
}

static udt_status sim_set_baud(udt_transport *t, uint32_t baud)
{
    (void)t; (void)baud;
    return UDT_OK;   /* no-op, accepted */
}

static const char *sim_name(udt_transport *t)
{
    return ((sim_ctx *)t->ctx)->name;
}

static void sim_close(udt_transport *t)
{
    if (t) {
        free(t->ctx);
        free(t);
    }
}

udt_status udt_sim_open(const sim_config *cfg, udt_transport **out)
{
    sim_ctx *c;
    udt_transport *t;

    if (!out) return UDT_ERR_ARG;
    *out = NULL;

    c = (sim_ctx *)calloc(1, sizeof(*c));
    t = (udt_transport *)calloc(1, sizeof(*t));
    if (!c || !t) { free(c); free(t); return UDT_ERR_NOMEM; }

    if (cfg) c->cfg = *cfg;
    if (!c->cfg.chip_name) c->cfg.chip_name = "SIM-SC6531E";
    if (!c->cfg.flash_size) c->cfg.flash_size = 8 * 1024 * 1024;

    snprintf(c->name, sizeof(c->name), "SIM:%s", c->cfg.chip_name);

    t->ctx = c;
    t->write = sim_write;
    t->read = sim_read;
    t->flush = sim_flush;
    t->set_baud = sim_set_baud;
    t->name = sim_name;
    t->close = sim_close;

    LOGI("Simulated device open: %s (%u bytes flash)",
         c->name, (unsigned)c->cfg.flash_size);
    *out = t;
    return UDT_OK;
}

int udt_transport_is_sim(const udt_transport *t)
{
    if (!t || !t->name) return 0;
    {
        const char *n = t->name((udt_transport *)t);
        return n && strncmp(n, "SIM:", 4) == 0;
    }
}
