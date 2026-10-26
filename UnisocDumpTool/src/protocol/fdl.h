/*
 * fdl.h - Flash Download Loader (FDL) staging.
 *
 * On Spreadtrum/Unisoc, the Boot ROM can only receive a small loader into SRAM
 * and run it. Real flash access is done by that loader:
 *
 *   FDL1 - tiny loader, runs from the Boot ROM, initialises DRAM.
 *   FDL2 - larger loader, runs from DRAM, exposes the flash read/write commands
 *          (including the partitioned READ_FLASH_START/MIDST/END used to dump).
 *
 * FDL1/FDL2 images are chip-specific and proprietary. They are NOT included in
 * this project and must be supplied by the user for their exact chip. This
 * module only *stages* an FDL the user provides; it does not and cannot invent
 * one. See docs/MISSING.md for what is required per chip.
 *
 * The staging sequence for one FDL image at load address 'addr':
 *   BSL_CMD_START_DATA { addr(4 BE), size(4 BE) }   -> ACK
 *   BSL_CMD_MIDST_DATA { chunk }                     -> ACK   (repeated)
 *   BSL_CMD_END_DATA                                 -> ACK
 *   BSL_CMD_EXEC_DATA                                -> ACK   (jumps to FDL)
 */
#ifndef UDT_FDL_H
#define UDT_FDL_H

#include <stddef.h>
#include <stdint.h>

#include "sprd_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *data;
    size_t         size;
    uint32_t       load_addr;   /* where the Boot ROM/FDL1 should place it */
} fdl_image;

/* Cancellation hook: return non-zero to abort a long staging loop. */
typedef int (*fdl_cancel_fn)(void *user);

/*
 * Stage one FDL image: START_DATA / MIDST_DATA.. / END_DATA / EXEC_DATA.
 * chunk_size is the download chunk (commonly 528 or 2112 bytes; 0 picks a safe
 * default). After EXEC_DATA the device is running the new loader.
 */
udt_status fdl_stage(sprd_session *s, const fdl_image *img, size_t chunk_size,
                     fdl_cancel_fn cancel, void *cancel_user);

/*
 * Convenience: the usual two-stage bring-up. Loads FDL1, execs it, then loads
 * FDL2 and execs it, switching the checksum mode as the loaders expect. Either
 * image may be NULL to skip (e.g. some chips need only one). Returns UDT_OK
 * only when the requested stage(s) all acknowledged.
 */
udt_status fdl_bringup(sprd_session *s,
                       const fdl_image *fdl1, const fdl_image *fdl2,
                       fdl_cancel_fn cancel, void *cancel_user);

/* Load an FDL image file from disk into a malloc'd buffer (caller frees img->data). */
udt_status fdl_load_file(const char *path, uint32_t load_addr, fdl_image *img);
void       fdl_free(fdl_image *img);

#ifdef __cplusplus
}
#endif

#endif /* UDT_FDL_H */
