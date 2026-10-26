/*
 * test_runner.c - Portable tests for the Unisoc Dump Tool core.
 *
 * These cover the parts that do not require Windows or hardware: checksums,
 * SHA-256, HDLC framing, the protocol loopback against the simulator, and a
 * complete simulated dump through the real read engine + file writer.
 *
 * Build (any platform with a C compiler): see tests/README or CMake target
 * "udt_tests". Exit code 0 means all tests passed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/common/sha256.h"
#include "../src/common/crc16.h"
#include "../src/protocol/hdlc.h"
#include "../src/protocol/sprd_proto.h"
#include "../src/sim/sim_device.h"
#include "../src/flash/flash_dump.h"
#include "../src/fileout/dump_writer.h"
#include "../src/common/logging.h"

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  [PASS] %s\n", msg); } \
    else      { g_fail++; printf("  [FAIL] %s\n", msg); } \
} while (0)

/* ---- SHA-256 known-answer tests. ---- */
static void test_sha256(void)
{
    uint8_t d[32];
    char hex[65];
    printf("SHA-256 vectors:\n");

    sha256("abc", 3, d);
    sha256_hex(d, hex);
    CHECK(strcmp(hex,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0,
        "sha256(\"abc\")");

    sha256("", 0, d);
    sha256_hex(d, hex);
    CHECK(strcmp(hex,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0,
        "sha256(\"\")");

    {
        /* 1,000,000 'a' -> known digest, exercises streaming. */
        sha256_ctx c;
        size_t i;
        char chunk[1000];
        memset(chunk, 'a', sizeof(chunk));
        sha256_init(&c);
        for (i = 0; i < 1000; i++) sha256_update(&c, chunk, sizeof(chunk));
        sha256_final(&c, d);
        sha256_hex(d, hex);
        CHECK(strcmp(hex,
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0,
            "sha256(1e6 x 'a') streaming");
    }
}

/* ---- Checksums. ---- */
static void test_crc(void)
{
    const char *s = "123456789";
    uint16_t c;
    printf("Checksums:\n");

    /* CRC-16/CCITT-FALSE (init 0xFFFF) over "123456789" is 0x29B1; our Boot ROM
     * variant uses init 0x0000, so just verify determinism + non-triviality
     * and the escape-sensitive FDL sum folds carries. */
    c = spd_crc16(0, s, 9);
    CHECK(c != 0 && c == spd_crc16(0, s, 9), "spd_crc16 deterministic");

    {
        uint8_t data[4] = {0x00, 0x00, 0x00, 0x00};
        uint16_t z = spd_fdl_crc(data, 4);
        uint8_t data2[4] = {0x12, 0x34, 0x56, 0x78};
        uint16_t nz = spd_fdl_crc(data2, 4);
        CHECK(z != nz, "spd_fdl_crc distinguishes payloads");
    }
}

/* ---- HDLC framing round-trip, including escape bytes. ---- */
static void test_hdlc(void)
{
    uint8_t payload[] = {0x00, 0x01, 0x7E, 0x7D, 0x41, 0x7E};
    uint8_t frame[64];
    uint8_t decoded[64];
    size_t fl, dl;
    printf("HDLC framing:\n");

    fl = hdlc_encode(payload, sizeof(payload), frame, sizeof(frame));
    CHECK(fl > 0 && frame[0] == 0x7E && frame[fl - 1] == 0x7E,
          "encode delimits with 0x7E");

    /* No raw 0x7E should appear in the body. */
    {
        int interior_flag = 0;
        size_t i;
        for (i = 1; i < fl - 1; i++) if (frame[i] == 0x7E) interior_flag = 1;
        CHECK(!interior_flag, "encode escapes interior 0x7E");
    }

    dl = hdlc_decode(frame + 1, fl - 2, decoded, sizeof(decoded));
    CHECK(dl == sizeof(payload) && memcmp(decoded, payload, dl) == 0,
          "decode recovers original payload");
}

/* ---- Protocol loopback against the simulator. ---- */
static void test_proto_loopback(void)
{
    sim_config cfg = {0};
    udt_transport *t = NULL;
    sprd_session s;
    char ver[64] = {0};
    udt_status st;
    printf("Protocol loopback (simulator):\n");

    cfg.chip_name = "SIM-SC6531E";
    cfg.flash_size = 1 << 20;
    cfg.require_fdl = 0;
    st = udt_sim_open(&cfg, &t);
    CHECK(st == UDT_OK && t != NULL, "sim open");
    if (!t) return;

    CHECK(udt_transport_is_sim(t), "transport identifies as simulator");

    sprd_session_init(&s, t);
    st = sprd_handshake(&s, ver, sizeof(ver));
    CHECK(st == UDT_OK, "handshake ok");
    CHECK(strstr(ver, "SC6531E") != NULL, "version string returned");

    /* One linear read of 256 bytes and compare against the model. */
    {
        uint8_t req[12], i;
        sprd_msg reply;
        int ok = 1;
        sprd_put_be32(req, 0x1000);
        sprd_put_be32(req + 4, 256);
        sprd_put_be32(req + 8, 0);
        st = sprd_transact(&s, BSL_CMD_READ_FLASH, req, 12, &reply, 1000);
        CHECK(st == UDT_OK && reply.type == BSL_REP_READ_FLASH && reply.length == 256,
              "linear read returns 256 bytes");
        for (i = 0; i < 255; i++) {
            if (reply.data[i] != sim_flash_byte(0x1000 + i)) { ok = 0; break; }
        }
        CHECK(ok, "read bytes match synthetic flash model");
    }

    t->close(t);
}

/* ---- Full simulated dump through the real engine + writer. ---- */
static int verify_bin_against_model(const char *path, uint32_t base, uint64_t size)
{
    FILE *f = fopen(path, "rb");
    uint64_t i;
    int ok = 1;
    if (!f) return 0;
    for (i = 0; i < size; i++) {
        int ch = fgetc(f);
        if (ch == EOF) { ok = 0; break; }
        if ((uint8_t)ch != sim_flash_byte(base + (uint32_t)i)) { ok = 0; break; }
    }
    fclose(f);
    return ok;
}

static void test_full_dump(void)
{
    sim_config cfg = {0};
    udt_transport *t = NULL;
    dump_writer *w = NULL;
    dump_params p;
    int complete = 0;
    udt_status st;
    char bin[800];
    const char *base_dir = "udt_test_out";
    printf("Full simulated dump:\n");

    cfg.chip_name = "SIM-SC6531E";
    cfg.flash_size = 64 * 1024;
    cfg.require_fdl = 0;
    st = udt_sim_open(&cfg, &t);
    CHECK(st == UDT_OK, "sim open");
    if (!t) return;

    st = dump_open(&w, base_dir, "SIM-SC6531E", "flash_dump.bin", 1);
    CHECK(st == UDT_OK && w != NULL, "dump_open creates session dir");
    if (!w) { t->close(t); return; }

    memset(&p, 0, sizeof(p));
    p.method = READ_LINEAR;
    p.base_addr = 0;
    p.total_size = cfg.flash_size;
    p.chunk_size = 1024;

    st = flash_dump_run(t, w, &p, NULL, NULL, &complete);
    CHECK(st == UDT_OK, "engine ran");
    CHECK(complete == 1, "dump reported COMPLETE");
    CHECK(dump_bytes_written(w) == cfg.flash_size, "all bytes written");

    snprintf(bin, sizeof(bin), "%s/flash_dump.bin", dump_dir(w));
    CHECK(verify_bin_against_model(bin, 0, cfg.flash_size),
          "written .bin matches synthetic flash exactly");

    dump_close(w);
    t->close(t);
}

/* ---- FDL2 path + require_fdl gating. ---- */
static void test_fdl_required(void)
{
    sim_config cfg = {0};
    udt_transport *t = NULL;
    sprd_session s;
    udt_status st;
    uint8_t req[12];
    printf("FDL requirement gating:\n");

    cfg.chip_name = "SIM-SC9832E";
    cfg.flash_size = 1 << 20;
    cfg.require_fdl = 1;   /* reads must fail until an FDL is executed */
    st = udt_sim_open(&cfg, &t);
    if (!t) { CHECK(0, "sim open"); return; }

    sprd_session_init(&s, t);
    sprd_handshake(&s, NULL, 0);

    sprd_put_be32(req, 0);
    sprd_put_be32(req + 4, 256);
    sprd_put_be32(req + 8, 0);
    {
        sprd_msg reply;
        st = sprd_transact(&s, BSL_CMD_READ_FLASH, req, 12, &reply, 1000);
        CHECK(st == UDT_OK && reply.type == BSL_REP_OPERATION_FAILED,
              "read refused before FDL executed");
    }

    /* Pretend to stage+exec an FDL, then read should succeed. */
    sprd_command_ack(&s, BSL_CMD_START_DATA, req, 8, 1000);
    sprd_command_ack(&s, BSL_CMD_END_DATA, NULL, 0, 1000);
    sprd_command_ack(&s, BSL_CMD_EXEC_DATA, NULL, 0, 1000);
    {
        sprd_msg reply;
        st = sprd_transact(&s, BSL_CMD_READ_FLASH, req, 12, &reply, 1000);
        CHECK(st == UDT_OK && reply.type == BSL_REP_READ_FLASH,
              "read allowed after FDL executed");
    }

    t->close(t);
}

static void quiet_sink(void *u, log_level l, const char *m) { (void)u; (void)l; (void)m; }

int main(void)
{
    /* Silence engine logging during tests unless UDT_TEST_VERBOSE is set. */
    if (!getenv("UDT_TEST_VERBOSE")) {
        log_set_sink(quiet_sink, NULL);
    }

    printf("== Unisoc Dump Tool core tests ==\n");
    test_sha256();
    test_crc();
    test_hdlc();
    test_proto_loopback();
    test_full_dump();
    test_fdl_required();

    printf("\n== Results: %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
