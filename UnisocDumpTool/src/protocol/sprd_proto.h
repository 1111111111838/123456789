/*
 * sprd_proto.h - Spreadtrum/Unisoc BSL (Boot Stage Loader) download protocol.
 *
 * This layer builds and parses protocol messages and exchanges them as HDLC
 * frames over an abstract transport. It implements the publicly documented
 * Spreadtrum protocol used by the Boot ROM and by FDL1/FDL2 loaders.
 *
 * IMPORTANT HONESTY NOTE (see docs/PROTOCOL.md and docs/MISSING.md):
 *   - The command/response codes, framing and checksums below are the
 *     reverse-engineered, publicly documented protocol. They are not invented.
 *   - Actually reading flash on most chips requires a chip-specific FDL binary
 *     that is proprietary and is NOT shipped here. This layer can *load* an
 *     FDL the user supplies and then drive the read commands, but it cannot
 *     manufacture an FDL. Where an FDL is required and absent, the engine
 *     reports exactly that instead of pretending.
 */
#ifndef UDT_SPRD_PROTO_H
#define UDT_SPRD_PROTO_H

#include <stddef.h>
#include <stdint.h>

#include "../comm/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Host -> device commands (type field). ------------------------------ */
typedef enum {
    BSL_CMD_CONNECT          = 0x00,
    BSL_CMD_START_DATA       = 0x01, /* begin a download: addr(4 BE), size(4 BE) */
    BSL_CMD_MIDST_DATA       = 0x02, /* a chunk of download payload */
    BSL_CMD_END_DATA         = 0x03, /* finish a download */
    BSL_CMD_EXEC_DATA        = 0x04, /* jump to the just-downloaded code (run FDL) */
    BSL_CMD_NORMAL_RESET     = 0x05,
    BSL_CMD_READ_FLASH       = 0x06, /* legacy linear read: addr(4), size(4), pad(4) */
    BSL_CMD_CHANGE_BAUD      = 0x09,
    BSL_CMD_READ_FLASH_START = 0x10, /* FDL2 partitioned read: begin */
    BSL_CMD_READ_FLASH_MIDST = 0x11, /* FDL2 partitioned read: next chunk */
    BSL_CMD_READ_FLASH_END   = 0x12, /* FDL2 partitioned read: finish */
    BSL_CMD_KEEP_CHARGE      = 0x13,
    BSL_CMD_EXEC_NAND_INIT   = 0x21,
    BSL_CMD_READ_CHIP_TYPE   = 0x7C,
    BSL_CMD_READ_CHIP_UID    = 0x7D,
    BSL_CMD_DISABLE_TRANSCODE= 0x21  /* some FDLs: disable HDLC escaping (alias) */
} bsl_cmd;

/* ---- Device -> host responses (type field). ----------------------------- */
typedef enum {
    BSL_REP_ACK              = 0x80, /* generic success */
    BSL_REP_VER              = 0x81, /* version string (Boot ROM handshake reply) */
    BSL_REP_INVALID_CMD      = 0x82,
    BSL_REP_UNKNOWN_CMD      = 0x83,
    BSL_REP_OPERATION_FAILED = 0x84,
    BSL_REP_NOT_SUPPORT_BAUD = 0x85,
    BSL_REP_DOWN_NOT_START   = 0x86,
    BSL_REP_DOWN_MULTI_START = 0x87,
    BSL_REP_DOWN_EARLY_END   = 0x88,
    BSL_REP_DOWN_DEST_ERROR  = 0x89,
    BSL_REP_DOWN_SIZE_ERROR  = 0x8A,
    BSL_REP_VERIFY_ERROR     = 0x8B,
    BSL_REP_NOT_VERIFY       = 0x8C,
    BSL_REP_READ_FLASH       = 0x93, /* carries the read-back flash bytes */
    BSL_REP_READ_CHIP_TYPE   = 0x9D,
    BSL_REP_INCOMPATIBLE_PART= 0xA8,
    BSL_REP_UNKNOWN          = 0xFFFF
} bsl_rep;

#define SPRD_MAX_DATA 4096   /* max payload per message we build/accept here */

/* A decoded protocol message. */
typedef struct {
    uint16_t type;              /* bsl_cmd or bsl_rep */
    uint16_t length;            /* number of valid bytes in data */
    uint8_t  data[SPRD_MAX_DATA];
} sprd_msg;

/* Protocol session state carried alongside the transport. */
typedef struct {
    udt_transport *t;
    int use_fdl_crc;            /* 0 = Boot ROM CRC-16, 1 = FDL additive checksum */
    int transcode;             /* 1 = HDLC escaping active (default), 0 = raw */
    int default_timeout_ms;
} sprd_session;

void sprd_session_init(sprd_session *s, udt_transport *t);

/*
 * Boot ROM handshake. Sends the 0x7E "hello" bytes and waits for the version
 * reply (BSL_REP_VER). On success, ver_out (if non-NULL, capacity ver_cap) gets
 * the NUL-terminated version string the device returned. Uses Boot ROM CRC.
 */
udt_status sprd_handshake(sprd_session *s, char *ver_out, size_t ver_cap);

/* Build + frame + send one command; wait for and decode one reply frame. */
udt_status sprd_transact(sprd_session *s,
                         uint16_t cmd, const uint8_t *data, uint16_t len,
                         sprd_msg *reply, int timeout_ms);

/* Send a command and require a BSL_REP_ACK reply (common case). */
udt_status sprd_command_ack(sprd_session *s,
                            uint16_t cmd, const uint8_t *data, uint16_t len,
                            int timeout_ms);

/* Low-level: send a single already-built message as a frame. */
udt_status sprd_send(sprd_session *s, uint16_t type, const uint8_t *data, uint16_t len);

/* Low-level: read one frame and decode it into msg. */
udt_status sprd_recv(sprd_session *s, sprd_msg *msg, int timeout_ms);

/* Helpers for big-endian fields inside message payloads. */
void sprd_put_be32(uint8_t *p, uint32_t v);
uint32_t sprd_get_be32(const uint8_t *p);

#ifdef __cplusplus
}
#endif

#endif /* UDT_SPRD_PROTO_H */
