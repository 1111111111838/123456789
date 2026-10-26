/*
 * chipdb.h - Known Unisoc/Spreadtrum chip and USB-identity profiles.
 *
 * This is the extensibility point the specification asks for: new chip
 * generations are added as table rows, not as code changes elsewhere. Each
 * profile records what is *known* versus what must still be supplied (an FDL)
 * so the UI can honestly label a device as "supported", "unverified" or
 * "unsupported".
 */
#ifndef UDT_CHIPDB_H
#define UDT_CHIPDB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How confident we are that a full read can be performed. */
typedef enum {
    CAP_UNSUPPORTED = 0, /* protocol/read path not implemented for this chip */
    CAP_UNVERIFIED  = 1, /* protocol implemented but not confirmed on hardware,
                            or an FDL is required and not yet provided */
    CAP_SUPPORTED   = 2  /* read path implemented and confirmed on real hardware */
} dump_capability;

typedef enum {
    FLASH_UNKNOWN = 0,
    FLASH_NAND,
    FLASH_EMMC,
    FLASH_NOR,
    FLASH_SPI_NAND
} flash_kind;

typedef struct {
    const char *name;        /* e.g. "SC6531E" */
    const char *family;      /* e.g. "Mocor RTOS feature-phone" */
    uint16_t    usb_vid;     /* USB vendor id in download mode (0 = any) */
    uint16_t    usb_pid;     /* USB product id in download mode (0 = any) */
    flash_kind  typical_flash; /* best-effort; real kind is probed at runtime */
    uint32_t    fdl1_addr;   /* documented FDL1 load address (0 = unknown) */
    uint32_t    fdl2_addr;   /* documented FDL2 load address (0 = unknown) */
    int         needs_fdl;   /* 1 if flash read requires an external FDL */
    dump_capability capability;
    const char *notes;       /* honest caveats shown in the UI / logs */
} chip_profile;

/* Number of entries in the built-in table. */
int chipdb_count(void);

/* Get entry i (0..count-1), or NULL. */
const chip_profile *chipdb_at(int i);

/* Best match for a USB VID/PID, or NULL if nothing matches. */
const chip_profile *chipdb_by_usb(uint16_t vid, uint16_t pid);

/* Lookup by chip name (case-insensitive), or NULL. */
const chip_profile *chipdb_by_name(const char *name);

/* A generic "unknown Unisoc" profile for devices we recognise only loosely. */
const chip_profile *chipdb_generic(void);

const char *flash_kind_str(flash_kind k);
const char *capability_str(dump_capability c);

#ifdef __cplusplus
}
#endif

#endif /* UDT_CHIPDB_H */
