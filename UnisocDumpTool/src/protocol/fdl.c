#include "fdl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/logging.h"

#define FDL_DEFAULT_CHUNK 528

udt_status fdl_stage(sprd_session *s, const fdl_image *img, size_t chunk_size,
                     fdl_cancel_fn cancel, void *cancel_user)
{
    uint8_t hdr[8];
    size_t off;
    udt_status st;

    if (!img || !img->data || img->size == 0) return UDT_ERR_ARG;
    if (chunk_size == 0) chunk_size = FDL_DEFAULT_CHUNK;
    if (chunk_size > SPRD_MAX_DATA) chunk_size = SPRD_MAX_DATA;

    /* START_DATA: load address, total size. */
    sprd_put_be32(hdr, img->load_addr);
    sprd_put_be32(hdr + 4, (uint32_t)img->size);
    LOGI("FDL: START_DATA addr=0x%08X size=%u", img->load_addr, (unsigned)img->size);
    st = sprd_command_ack(s, BSL_CMD_START_DATA, hdr, 8, s->default_timeout_ms);
    if (st != UDT_OK) {
        LOGE("FDL: START_DATA rejected (%s)", udt_status_str(st));
        return st;
    }

    /* MIDST_DATA: stream the body. */
    for (off = 0; off < img->size; off += chunk_size) {
        size_t n = img->size - off;
        if (n > chunk_size) n = chunk_size;

        if (cancel && cancel(cancel_user)) {
            LOGW("FDL: staging cancelled by user");
            return UDT_ERR_CANCELLED;
        }

        st = sprd_command_ack(s, BSL_CMD_MIDST_DATA, img->data + off,
                              (uint16_t)n, s->default_timeout_ms);
        if (st != UDT_OK) {
            LOGE("FDL: MIDST_DATA failed at offset %u (%s)",
                 (unsigned)off, udt_status_str(st));
            return st;
        }
    }

    /* END_DATA. */
    st = sprd_command_ack(s, BSL_CMD_END_DATA, NULL, 0, s->default_timeout_ms);
    if (st != UDT_OK) {
        LOGE("FDL: END_DATA failed (%s)", udt_status_str(st));
        return st;
    }

    /* EXEC_DATA: jump into the loader. Some loaders do not ACK EXEC because
     * they immediately re-initialise the link; treat a timeout right after a
     * clean END_DATA as a soft success and let the caller re-handshake. */
    st = sprd_command_ack(s, BSL_CMD_EXEC_DATA, NULL, 0, s->default_timeout_ms);
    if (st == UDT_ERR_TIMEOUT) {
        LOGW("FDL: EXEC_DATA did not ACK (loader likely jumped); continuing");
        return UDT_OK;
    }
    if (st != UDT_OK) {
        LOGE("FDL: EXEC_DATA failed (%s)", udt_status_str(st));
        return st;
    }
    LOGI("FDL: staged and executing");
    return UDT_OK;
}

udt_status fdl_bringup(sprd_session *s,
                       const fdl_image *fdl1, const fdl_image *fdl2,
                       fdl_cancel_fn cancel, void *cancel_user)
{
    udt_status st;

    if (fdl1) {
        LOGI("Bring-up: staging FDL1");
        st = fdl_stage(s, fdl1, 0, cancel, cancel_user);
        if (st != UDT_OK) return st;

        /* After FDL1 runs, most chips keep Boot ROM CRC until FDL2 is up.
         * Re-handshake so we are framed correctly with the running FDL1. */
        st = sprd_handshake(s, NULL, 0);
        if (st != UDT_OK) {
            LOGW("Bring-up: FDL1 handshake returned %s", udt_status_str(st));
        }
    }

    if (fdl2) {
        LOGI("Bring-up: staging FDL2");
        st = fdl_stage(s, fdl2, 0, cancel, cancel_user);
        if (st != UDT_OK) return st;

        /* FDL2 commonly switches to the additive FDL checksum. The exact
         * moment is loader-dependent; this is the typical behaviour and must
         * be verified per chip (docs/MISSING.md). */
        s->use_fdl_crc = 1;
        LOGI("Bring-up: switched to FDL checksum mode");
    }

    return UDT_OK;
}

udt_status fdl_load_file(const char *path, uint32_t load_addr, fdl_image *img)
{
    FILE *f;
    long size;
    uint8_t *buf;
    size_t rd;

    if (!path || !img) return UDT_ERR_ARG;
    memset(img, 0, sizeof(*img));

    f = fopen(path, "rb");
    if (!f) {
        LOGE("FDL: cannot open %s", path);
        return UDT_ERR_IO;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UDT_ERR_IO; }
    size = ftell(f);
    if (size <= 0) { fclose(f); return UDT_ERR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UDT_ERR_IO; }

    buf = (uint8_t *)malloc((size_t)size);
    if (!buf) { fclose(f); return UDT_ERR_NOMEM; }

    rd = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (rd != (size_t)size) { free(buf); return UDT_ERR_IO; }

    img->data = buf;
    img->size = (size_t)size;
    img->load_addr = load_addr;
    LOGI("FDL: loaded %s (%ld bytes) for addr 0x%08X", path, size, load_addr);
    return UDT_OK;
}

void fdl_free(fdl_image *img)
{
    if (img && img->data) {
        free((void *)img->data);
        img->data = NULL;
        img->size = 0;
    }
}
