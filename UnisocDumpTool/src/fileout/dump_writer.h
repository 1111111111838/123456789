/*
 * dump_writer.h - Writes dump data and its sidecar files to the session folder.
 *
 * Responsibilities:
 *   - own the session output directory (UnisocDumps/Device_<timestamp>/)
 *   - stream dump bytes to a .bin while hashing them with SHA-256 (no buffering
 *     of the whole dump in memory)
 *   - record every address range that was read successfully
 *   - write device_info.txt / partition_map.txt / dump_manifest.txt /
 *     dump_sha256.txt describing exactly what was captured
 *   - mark a dump Incomplete when ranges are missing or a disconnect happened
 *
 * The writer never fabricates files for data that was not collected.
 */
#ifndef UDT_DUMP_WRITER_H
#define UDT_DUMP_WRITER_H

#include <stddef.h>
#include <stdint.h>

#include "../comm/transport.h"
#include "../common/sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t start;
    uint64_t len;
} dump_range;

typedef struct dump_writer dump_writer;

/*
 * Create the session directory under base_dir and open the main dump file.
 * - base_dir: e.g. "C:/UnisocDumps" (created if absent)
 * - label:    short device label used in the folder name
 * - bin_name: the dump file name, e.g. "flash_dump.bin"
 * - simulated: 1 if the data comes from the simulator (recorded in manifest)
 * On success *out is set; release with dump_close().
 */
udt_status dump_open(dump_writer **out, const char *base_dir, const char *label,
                     const char *bin_name, int simulated);

/* Append dump bytes (also fed to the running SHA-256). */
udt_status dump_write(dump_writer *w, const uint8_t *data, size_t len);

/* Record a successfully read absolute range (merged with adjacent ranges). */
void dump_record_range(dump_writer *w, uint64_t start, uint64_t len);

/* Known total flash size, if the device reported it (0 = unknown). */
void dump_set_total(dump_writer *w, uint64_t total);

uint64_t dump_bytes_written(const dump_writer *w);
const char *dump_dir(const dump_writer *w);

/* Write a UTF-8 text sidecar file in the session dir. */
udt_status dump_write_text(dump_writer *w, const char *name, const char *content);

/*
 * Finalize: flush the bin, compute the final hash, write dump_sha256.txt and
 * dump_manifest.txt. 'complete' must be 1 only if the entire intended range was
 * read. extra_manifest (may be NULL) is appended into the manifest verbatim
 * (device info, per-range log, timings). sha_hex_out (optional, >=65 bytes)
 * receives the hex digest.
 */
udt_status dump_finalize(dump_writer *w, int complete,
                         const char *extra_manifest, char sha_hex_out[65]);

void dump_close(dump_writer *w);

#ifdef __cplusplus
}
#endif

#endif /* UDT_DUMP_WRITER_H */
