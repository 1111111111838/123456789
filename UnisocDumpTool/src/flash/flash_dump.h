/*
 * flash_dump.h - The real flash read engine.
 *
 * Given an already-open transport (real serial OR simulator) and an open
 * dump_writer, this drives the device:
 *   1. Boot ROM handshake.
 *   2. Optional FDL1/FDL2 bring-up from user-supplied images.
 *   3. Flash readout using the method appropriate to the device:
 *        - FDL2 partitioned read (READ_FLASH_START/MIDST/END), or
 *        - legacy linear read (READ_FLASH by address).
 *   4. Stream every chunk straight to disk via the writer, hashing as it goes.
 *   5. Track read ranges, progress, speed; support controlled cancellation;
 *      detect short reads / missing blocks and mark the dump incomplete.
 *
 * The engine reads real bytes from the device. It performs NO writes, erases,
 * formats, repartitioning or firmware flashing - only reads.
 */
#ifndef UDT_FLASH_DUMP_H
#define UDT_FLASH_DUMP_H

#include <stddef.h>
#include <stdint.h>

#include "../comm/transport.h"
#include "../fileout/dump_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    READ_AUTO = 0,   /* pick FDL2 if an FDL2 image is provided, else linear */
    READ_LINEAR,     /* BSL_CMD_READ_FLASH by address */
    READ_FDL2        /* BSL_CMD_READ_FLASH_START/MIDST/END */
} read_method;

typedef struct {
    read_method method;

    uint32_t base_addr;     /* first address / partition base to read */
    uint64_t total_size;    /* number of bytes to read (required; 0 -> error) */
    uint32_t chunk_size;    /* bytes per read request (0 -> safe default) */

    /* Optional FDL staging. If a path is NULL that stage is skipped. */
    const char *fdl1_path;
    uint32_t    fdl1_addr;
    const char *fdl2_path;
    uint32_t    fdl2_addr;

    uint32_t baud;          /* link speed hint (serial only) */

    /* Cancellation: set *cancel non-zero from another thread to abort cleanly.
     * May be NULL. Partial data captured so far is kept and marked incomplete. */
    volatile int *cancel;
} dump_params;

typedef struct {
    uint64_t bytes_done;
    uint64_t bytes_total;
    double   speed_bps;     /* instantaneous-ish bytes/sec */
    int      percent;       /* 0..100, or -1 if total unknown */
} dump_progress;

/* Called periodically. Return non-zero to request cancellation. May be NULL. */
typedef int (*progress_fn)(void *user, const dump_progress *p);

/*
 * Run the whole readout. On return *out_complete (if non-NULL) is 1 only if the
 * full requested range was read. Returns UDT_OK if the engine ran to a clean
 * stop (complete or a cleanly-handled partial), or a negative udt_status for a
 * hard failure. A cancelled or short read still writes/finalizes what was read.
 */
udt_status flash_dump_run(udt_transport *t, dump_writer *w,
                          const dump_params *params,
                          progress_fn progress, void *user,
                          int *out_complete);

#ifdef __cplusplus
}
#endif

#endif /* UDT_FLASH_DUMP_H */
