/* Request localtime_r / POSIX declarations on glibc-based builds. */
#if !defined(_WIN32)
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#  endif
#endif

#include "dump_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/logging.h"

#if defined(_WIN32)
#  include <direct.h>
#  define UDT_MKDIR(p) _mkdir(p)
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  define UDT_MKDIR(p) mkdir((p), 0775)
#endif

#define RANGES_INIT 16

struct dump_writer {
    char      dir[640];
    char      bin_path[700];
    FILE     *bin;
    sha256_ctx sha;
    uint64_t  written;
    uint64_t  total;
    int       simulated;

    dump_range *ranges;
    size_t      nranges;
    size_t      capranges;
};

static int ensure_dir(const char *path)
{
    int r = UDT_MKDIR(path);
    if (r == 0) return 0;
    /* EEXIST is fine. */
    return 0; /* best-effort; the fopen below surfaces real failures */
}

udt_status dump_open(dump_writer **out, const char *base_dir, const char *label,
                     const char *bin_name, int simulated)
{
    dump_writer *w;
    time_t now;
    struct tm tmv;
    char stamp[32];
    char safe[64];
    size_t i;

    if (!out || !base_dir || !bin_name) return UDT_ERR_ARG;
    *out = NULL;

    w = (dump_writer *)calloc(1, sizeof(*w));
    if (!w) return UDT_ERR_NOMEM;

    w->ranges = (dump_range *)malloc(sizeof(dump_range) * RANGES_INIT);
    if (!w->ranges) { free(w); return UDT_ERR_NOMEM; }
    w->capranges = RANGES_INIT;
    w->simulated = simulated;

    sha256_init(&w->sha);

    now = time(NULL);
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H%M%S", &tmv);

    /* Sanitise label for a folder name. */
    if (!label || !label[0]) label = "Device";
    for (i = 0; label[i] && i < sizeof(safe) - 1; i++) {
        char ch = label[i];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')
            safe[i] = ch;
        else
            safe[i] = '_';
    }
    safe[i] = '\0';

    ensure_dir(base_dir);
    snprintf(w->dir, sizeof(w->dir), "%s/%s_%s", base_dir, safe, stamp);
    if (ensure_dir(w->dir) != 0) {
        LOGE("Cannot create output directory %s", w->dir);
        free(w->ranges); free(w);
        return UDT_ERR_IO;
    }

    snprintf(w->bin_path, sizeof(w->bin_path), "%s/%s", w->dir, bin_name);
    w->bin = fopen(w->bin_path, "wb");
    if (!w->bin) {
        LOGE("Cannot create dump file %s", w->bin_path);
        free(w->ranges); free(w);
        return UDT_ERR_IO;
    }

    LOGI("Output folder: %s", w->dir);
    *out = w;
    return UDT_OK;
}

udt_status dump_write(dump_writer *w, const uint8_t *data, size_t len)
{
    if (!w || !w->bin) return UDT_ERR_ARG;
    if (len == 0) return UDT_OK;
    if (fwrite(data, 1, len, w->bin) != len) {
        LOGE("Write failed (disk full?) after %llu bytes",
             (unsigned long long)w->written);
        return UDT_ERR_IO;
    }
    sha256_update(&w->sha, data, len);
    w->written += len;
    return UDT_OK;
}

void dump_record_range(dump_writer *w, uint64_t start, uint64_t len)
{
    if (!w || len == 0) return;

    /* Merge with the last range if contiguous (the common case). */
    if (w->nranges > 0) {
        dump_range *last = &w->ranges[w->nranges - 1];
        if (last->start + last->len == start) {
            last->len += len;
            return;
        }
    }
    if (w->nranges == w->capranges) {
        size_t nc = w->capranges * 2;
        dump_range *nr = (dump_range *)realloc(w->ranges, sizeof(dump_range) * nc);
        if (!nr) return;   /* best-effort logging; data already written */
        w->ranges = nr;
        w->capranges = nc;
    }
    w->ranges[w->nranges].start = start;
    w->ranges[w->nranges].len = len;
    w->nranges++;
}

void dump_set_total(dump_writer *w, uint64_t total) { if (w) w->total = total; }
uint64_t dump_bytes_written(const dump_writer *w) { return w ? w->written : 0; }
const char *dump_dir(const dump_writer *w) { return w ? w->dir : ""; }

udt_status dump_write_text(dump_writer *w, const char *name, const char *content)
{
    char path[720];
    FILE *f;
    if (!w || !name) return UDT_ERR_ARG;
    snprintf(path, sizeof(path), "%s/%s", w->dir, name);
    f = fopen(path, "wb");
    if (!f) return UDT_ERR_IO;
    if (content && *content) fputs(content, f);
    fclose(f);
    return UDT_OK;
}

udt_status dump_finalize(dump_writer *w, int complete,
                         const char *extra_manifest, char sha_hex_out[65])
{
    uint8_t digest[SHA256_DIGEST_SIZE];
    char hex[65];
    char line[256];
    char *manifest;
    size_t mcap, mlen;
    size_t i;

    if (!w) return UDT_ERR_ARG;

    if (w->bin) { fflush(w->bin); fclose(w->bin); w->bin = NULL; }

    sha256_final(&w->sha, digest);
    sha256_hex(digest, hex);
    if (sha_hex_out) memcpy(sha_hex_out, hex, 65);

    /* dump_sha256.txt (sha256sum-compatible: "<hash>  <file>"). */
    {
        const char *slash = strrchr(w->bin_path, '/');
        const char *bn = slash ? slash + 1 : w->bin_path;
        char body[800];
        snprintf(body, sizeof(body), "%s  %s\n", hex, bn);
        dump_write_text(w, "dump_sha256.txt", body);
    }

    /* Build manifest. */
    mcap = 4096 + w->nranges * 64 + (extra_manifest ? strlen(extra_manifest) : 0);
    manifest = (char *)malloc(mcap);
    if (!manifest) return UDT_ERR_NOMEM;
    mlen = 0;

#define APP(...) do { \
        int _n = snprintf(line, sizeof(line), __VA_ARGS__); \
        if (_n > 0 && mlen + (size_t)_n < mcap) { memcpy(manifest + mlen, line, _n); mlen += _n; } \
    } while (0)

    APP("Unisoc Dump Tool - dump manifest\n");
    APP("================================\n");
    APP("mode: %s\n", w->simulated ? "SIMULATED (not real hardware)" : "REAL HARDWARE");
    APP("status: %s\n", complete ? "COMPLETE" : "INCOMPLETE");
    APP("bytes_read: %llu\n", (unsigned long long)w->written);
    if (w->total)
        APP("flash_total: %llu\n", (unsigned long long)w->total);
    else
        APP("flash_total: unknown\n");
    APP("sha256: %s\n", hex);
    APP("\nsuccessful_ranges (start, length):\n");
    for (i = 0; i < w->nranges; i++) {
        APP("  0x%08llX  %llu\n",
            (unsigned long long)w->ranges[i].start,
            (unsigned long long)w->ranges[i].len);
    }
    if (extra_manifest && mlen + strlen(extra_manifest) + 2 < mcap) {
        manifest[mlen++] = '\n';
        memcpy(manifest + mlen, extra_manifest, strlen(extra_manifest));
        mlen += strlen(extra_manifest);
    }
    manifest[mlen < mcap ? mlen : mcap - 1] = '\0';
#undef APP

    dump_write_text(w, "dump_manifest.txt", manifest);
    free(manifest);

    if (!complete) {
        /* Leave an unmistakable marker alongside the data. */
        dump_write_text(w, "INCOMPLETE.txt",
            "This dump is INCOMPLETE. Some address ranges were not read.\n"
            "See dump_manifest.txt for the ranges that were captured.\n");
    }

    LOGI("Finalized: %llu bytes, sha256=%s, %s",
         (unsigned long long)w->written, hex,
         complete ? "COMPLETE" : "INCOMPLETE");
    return UDT_OK;
}

void dump_close(dump_writer *w)
{
    if (!w) return;
    if (w->bin) fclose(w->bin);
    free(w->ranges);
    free(w);
}
