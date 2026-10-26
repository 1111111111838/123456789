/*
 * device_detect.h - Automatic detection of a connected Unisoc download port.
 *
 * On Windows this enumerates USB/serial devices via SetupAPI, extracts VID/PID
 * and the COM port the download driver created, and matches against chipdb.
 * On other platforms a stub is provided so the portable core and tests build.
 *
 * Detection never claims more than it knows: every field is tagged as either
 * confirmed (read from the device/registry) or inferred (matched from a table).
 */
#ifndef UDT_DEVICE_DETECT_H
#define UDT_DEVICE_DETECT_H

#include <stdint.h>

#include "chipdb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UDT_NAME_MAX 128
#define UDT_PATH_MAX 512

typedef struct {
    int      present;                 /* 1 if a candidate device was found */
    uint16_t usb_vid;
    uint16_t usb_pid;
    int      usb_ids_confirmed;       /* 1 = read from the OS, 0 = unknown */

    char     com_port[16];            /* e.g. "COM7", or "" if none */
    char     friendly_name[UDT_NAME_MAX];
    char     device_path[UDT_PATH_MAX];

    const chip_profile *chip;         /* matched profile, or chipdb_generic() */
    int      chip_confirmed;          /* 1 = identified from device, 0 = inferred */
} udt_device_info;

/*
 * Scan once for a Unisoc download port. Returns 1 and fills 'out' if one is
 * found, 0 if none, negative udt_status on error. Safe to call repeatedly
 * (the UI polls it).
 */
int udt_detect_once(udt_device_info *out);

/* Pretty one-line summary into buf. */
void udt_device_summary(const udt_device_info *info, char *buf, int cap);

#ifdef __cplusplus
}
#endif

#endif /* UDT_DEVICE_DETECT_H */
