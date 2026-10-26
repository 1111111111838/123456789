#include "flash_dump.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../protocol/sprd_proto.h"
#include "../protocol/fdl.h"
#include "../common/logging.h"

#define DEFAULT_CHUNK 1024

static int cancel_adapter(void *user)
{
    volatile int *flag = (volatile int *)user;
    return flag && *flag;
}

static double now_sec(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static void report(progress_fn cb, void *user, uint64_t done, uint64_t total,
                   double speed, int *cancel_req)
{
    if (!cb) return;
    {
        dump_progress p;
        p.bytes_done = done;
        p.bytes_total = total;
        p.speed_bps = speed;
        p.percent = total ? (int)((done * 100) / total) : -1;
        if (cb(user, &p) && cancel_req) *cancel_req = 1;
    }
}

/* One flash read request. Returns bytes placed in buf (<= want), or negative. */
static int read_one(sprd_session *s, read_method method,
                    uint32_t base, uint32_t offset, uint32_t want,
                    uint8_t *buf)
{
    uint8_t req[12];
    sprd_msg reply;
    udt_status st;

    if (method == READ_LINEAR) {
        sprd_put_be32(req, base + offset);
        sprd_put_be32(req + 4, want);
        sprd_put_be32(req + 8, 0);
        st = sprd_transact(s, BSL_CMD_READ_FLASH, req, 12, &reply,
                           s->default_timeout_ms);
    } else {
        sprd_put_be32(req, want);
        sprd_put_be32(req + 4, offset);
        st = sprd_transact(s, BSL_CMD_READ_FLASH_MIDST, req, 8, &reply,
                           s->default_timeout_ms);
    }

    if (st != UDT_OK) return (int)st;
    if (reply.type != BSL_REP_READ_FLASH) {
        LOGW("read at offset %u: unexpected reply %04X", offset, reply.type);
        return UDT_ERR_NACK;
    }
    if (reply.length == 0) return 0;
    memcpy(buf, reply.data, reply.length);
    return (int)reply.length;
}

udt_status flash_dump_run(udt_transport *t, dump_writer *w,
                          const dump_params *params,
                          progress_fn progress, void *user,
                          int *out_complete)
{
    sprd_session s;
    read_method method;
    uint32_t chunk;
    uint64_t total, done = 0;
    int cancel_req = 0;
    char ver[128] = {0};
    udt_status st;
    double t_start, t_last;
    uint64_t last_bytes = 0;
    fdl_image fdl1 = {0}, fdl2 = {0};
    int have_fdl1 = 0, have_fdl2 = 0;
    char extra[512];
    int complete = 0;

    if (out_complete) *out_complete = 0;
    if (!t || !w || !params) return UDT_ERR_ARG;

    total = params->total_size;
    if (total == 0) {
        LOGE("Refusing to run: total read size is unknown. Provide the flash "
             "size (from the device/PAC) so the dump has a defined extent.");
        return UDT_ERR_ARG;
    }
    chunk = params->chunk_size ? params->chunk_size : DEFAULT_CHUNK;
    if (chunk > SPRD_MAX_DATA) chunk = SPRD_MAX_DATA;

    sprd_session_init(&s, t);
    dump_set_total(w, total);

    /* 1. Handshake. */
    LOGI("Handshaking with Boot ROM...");
    st = sprd_handshake(&s, ver, sizeof(ver));
    if (st != UDT_OK) {
        LOGE("Handshake failed: %s", udt_status_str(st));
        return st;
    }
    if (ver[0]) LOGI("Device version: %s", ver);

    /* 2. Optional FDL bring-up. */
    if (params->fdl1_path) {
        if (fdl_load_file(params->fdl1_path, params->fdl1_addr, &fdl1) == UDT_OK)
            have_fdl1 = 1;
        else
            LOGW("FDL1 not loaded; continuing without it");
    }
    if (params->fdl2_path) {
        if (fdl_load_file(params->fdl2_path, params->fdl2_addr, &fdl2) == UDT_OK)
            have_fdl2 = 1;
        else
            LOGW("FDL2 not loaded; continuing without it");
    }
    if (have_fdl1 || have_fdl2) {
        st = fdl_bringup(&s, have_fdl1 ? &fdl1 : NULL, have_fdl2 ? &fdl2 : NULL,
                         cancel_adapter, (void *)params->cancel);
        if (st != UDT_OK) {
            LOGE("FDL bring-up failed: %s", udt_status_str(st));
            if (have_fdl1) fdl_free(&fdl1);
            if (have_fdl2) fdl_free(&fdl2);
            return st;
        }
    }

    /* 3. Decide method. */
    method = params->method;
    if (method == READ_AUTO) {
        method = have_fdl2 ? READ_FDL2 : READ_LINEAR;
    }
    LOGI("Read method: %s, base=0x%08X, total=%llu, chunk=%u",
         method == READ_FDL2 ? "FDL2 partitioned" : "linear",
         params->base_addr, (unsigned long long)total, (unsigned)chunk);

    if (method == READ_FDL2 && !have_fdl2) {
        LOGW("FDL2 read requested without an FDL2 image. Most chips cannot "
             "satisfy this; the device will likely reject the read. See "
             "docs/MISSING.md.");
    }

    /* For FDL2, open the partitioned read. */
    if (method == READ_FDL2) {
        uint8_t req[12];
        sprd_put_be32(req, params->base_addr);
        sprd_put_be32(req + 4, (uint32_t)total);
        sprd_put_be32(req + 8, 0);
        st = sprd_command_ack(&s, BSL_CMD_READ_FLASH_START, req, 12,
                              s.default_timeout_ms);
        if (st != UDT_OK) {
            LOGE("READ_FLASH_START rejected: %s", udt_status_str(st));
            if (have_fdl1) fdl_free(&fdl1);
            if (have_fdl2) fdl_free(&fdl2);
            return st;
        }
    }

    /* 4. Read loop. */
    t_start = t_last = now_sec();
    while (done < total) {
        uint8_t buf[SPRD_MAX_DATA];
        uint32_t want = (uint32_t)((total - done) < chunk ? (total - done) : chunk);
        int got;

        if ((params->cancel && *params->cancel) || cancel_req) {
            LOGW("Cancellation requested at %llu/%llu bytes",
                 (unsigned long long)done, (unsigned long long)total);
            break;
        }

        got = read_one(&s, method, params->base_addr, (uint32_t)done, want, buf);
        if (got < 0) {
            LOGE("Read error at offset %llu: %s",
                 (unsigned long long)done, udt_status_str((udt_status)got));
            break;   /* keep what we have; mark incomplete */
        }
        if (got == 0) {
            LOGE("Device returned 0 bytes at offset %llu (short read / missing "
                 "block). Stopping.", (unsigned long long)done);
            break;
        }

        st = dump_write(w, buf, (size_t)got);
        if (st != UDT_OK) {
            LOGE("Failed to write dump data: %s", udt_status_str(st));
            break;
        }
        dump_record_range(w, params->base_addr + done, (uint64_t)got);
        done += (uint64_t)got;

        /* Progress + speed roughly every 250 ms. */
        {
            double tn = now_sec();
            if (tn - t_last >= 0.25 || done >= total) {
                double dt = tn - t_last;
                double speed = dt > 0 ? (double)(done - last_bytes) / dt : 0.0;
                report(progress, user, done, total, speed, &cancel_req);
                t_last = tn;
                last_bytes = done;
            }
        }
    }

    /* For FDL2, close the partitioned read regardless. */
    if (method == READ_FDL2) {
        udt_status e = sprd_command_ack(&s, BSL_CMD_READ_FLASH_END, NULL, 0,
                                        s.default_timeout_ms);
        if (e != UDT_OK) LOGW("READ_FLASH_END returned %s", udt_status_str(e));
    }

    complete = (done == total);

    /* 5. Finalize. */
    {
        double elapsed = now_sec() - t_start;
        snprintf(extra, sizeof(extra),
                 "device_version: %s\n"
                 "read_method: %s\n"
                 "base_addr: 0x%08X\n"
                 "elapsed_seconds: %.2f\n"
                 "avg_speed_bytes_per_sec: %.0f\n"
                 "fdl1: %s\nfdl2: %s\n",
                 ver[0] ? ver : "(none)",
                 method == READ_FDL2 ? "FDL2 partitioned" : "linear",
                 params->base_addr, elapsed,
                 elapsed > 0 ? (double)done / elapsed : 0.0,
                 have_fdl1 ? params->fdl1_path : "(none)",
                 have_fdl2 ? params->fdl2_path : "(none)");
    }
    dump_finalize(w, complete, extra, NULL);

    if (have_fdl1) fdl_free(&fdl1);
    if (have_fdl2) fdl_free(&fdl2);

    if (out_complete) *out_complete = complete;

    if (complete) {
        LOGI("Dump COMPLETE: %llu bytes", (unsigned long long)done);
    } else {
        LOGW("Dump INCOMPLETE: %llu of %llu bytes",
             (unsigned long long)done, (unsigned long long)total);
    }
    return UDT_OK;
}
