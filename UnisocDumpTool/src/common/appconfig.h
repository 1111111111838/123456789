/*
 * appconfig.h - Reads real-dump parameters from UnisocDumpTool.ini.
 *
 * Real flash reads need values this tool must NOT invent: the flash size, the
 * base address, and (for most chips) the FDL1/FDL2 images and their load
 * addresses. These come from the device's own flashing package (PAC) and are
 * supplied by the user through an INI next to the executable. If the INI is
 * missing or incomplete for real mode, the engine says exactly what is missing
 * instead of guessing (see docs/MISSING.md).
 *
 * Example UnisocDumpTool.ini:
 *   [flash]
 *   size      = 0x800000      ; 8 MiB
 *   base_addr = 0x00000000
 *   chunk     = 1024
 *   baud      = 115200
 *   method    = auto          ; auto | linear | fdl2
 *   [fdl]
 *   fdl1      = C:\unisoc\fdl1.bin
 *   fdl1_addr = 0x00005000
 *   fdl2      = C:\unisoc\fdl2.bin
 *   fdl2_addr = 0x009F0000
 */
#ifndef UDT_APPCONFIG_H
#define UDT_APPCONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int      have_size;
    uint64_t flash_size;
    uint32_t base_addr;
    uint32_t chunk;
    uint32_t baud;
    int      method;        /* 0 auto, 1 linear, 2 fdl2 */

    char     fdl1[512];
    uint32_t fdl1_addr;
    char     fdl2[512];
    uint32_t fdl2_addr;
} app_config;

/* Fill cfg with defaults, then overlay values from ini_path if it exists.
 * Returns 1 if a file was read, 0 if only defaults are in place. */
int appconfig_load(const char *ini_path, app_config *cfg);

/* Build the INI path next to the current executable (Windows). On other
 * platforms, returns "UnisocDumpTool.ini" in the cwd. buf cap bytes. */
void appconfig_default_path(char *buf, int cap);

#ifdef __cplusplus
}
#endif

#endif /* UDT_APPCONFIG_H */
