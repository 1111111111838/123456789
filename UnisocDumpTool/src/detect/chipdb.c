#include "chipdb.h"

#include <string.h>
#include <ctype.h>

/*
 * Built-in chip/USB profiles.
 *
 * Honesty rules applied here (per the specification):
 *   - capability is never "SUPPORTED" unless a full read was confirmed on real
 *     hardware. Until that documentation exists (docs/TESTED_DEVICES.md), the
 *     most optimistic value is "UNVERIFIED".
 *   - FDL load addresses are left 0 ("unknown") unless taken from a confirmed
 *     source. A 0 means: read the address from the device's own flashing
 *     package (PAC) and pass it in; do not guess. See docs/MISSING.md.
 *
 * The Spreadtrum download/diag USB identity (VID 0x1782, PID 0x4d00) is widely
 * documented and is matched generically below.
 */
static const chip_profile k_chips[] = {
    {
        "SC6531E", "Mocor RTOS feature-phone",
        0x1782, 0x4d00, FLASH_SPI_NAND,
        0, 0, 1, CAP_UNVERIFIED,
        "Common 2G feature-phone SoC. Flash read requires FDL1/FDL2 supplied by "
        "the user (extract from the matching PAC). Load addresses are not "
        "hard-coded; provide them from the PAC."
    },
    {
        "SC6531DA", "Mocor RTOS feature-phone",
        0x1782, 0x4d00, FLASH_NOR,
        0, 0, 1, CAP_UNVERIFIED,
        "2G feature-phone SoC, frequently SPI-NOR. Requires user-supplied FDL."
    },
    {
        "SC6530", "Mocor RTOS feature-phone",
        0x1782, 0x4d00, FLASH_NOR,
        0, 0, 1, CAP_UNVERIFIED,
        "Older 2G feature-phone SoC. Requires user-supplied FDL."
    },
    {
        "SC7731", "Unisoc smartphone (Android)",
        0x1782, 0x4d00, FLASH_EMMC,
        0, 0, 1, CAP_UNVERIFIED,
        "Android-class SoC with eMMC. Protocol is the same BSL family; read "
        "requires the matching FDL1/FDL2 and partition table."
    },
    {
        "SC9832E", "Unisoc smartphone (Android)",
        0x1782, 0x4d00, FLASH_EMMC,
        0, 0, 1, CAP_UNVERIFIED,
        "Android-class SoC with eMMC. Requires user-supplied FDL."
    },
};

static const chip_profile k_generic = {
    "Unknown Unisoc", "Unisoc/Spreadtrum (download mode)",
    0x1782, 0x0000, FLASH_UNKNOWN,
    0, 0, 1, CAP_UNVERIFIED,
    "Recognised as a Spreadtrum download port, but the exact SoC was not "
    "identified. A read needs the correct FDL for the real chip."
};

int chipdb_count(void)
{
    return (int)(sizeof(k_chips) / sizeof(k_chips[0]));
}

const chip_profile *chipdb_at(int i)
{
    if (i < 0 || i >= chipdb_count()) return NULL;
    return &k_chips[i];
}

const chip_profile *chipdb_by_usb(uint16_t vid, uint16_t pid)
{
    int i;
    const chip_profile *best = NULL;
    for (i = 0; i < chipdb_count(); i++) {
        const chip_profile *c = &k_chips[i];
        if (c->usb_vid && c->usb_vid != vid) continue;
        if (c->usb_pid && c->usb_pid != pid) continue;
        /* First exact VID/PID match wins; keep looking only if no PID set. */
        if (c->usb_pid == pid) return c;
        if (!best) best = c;
    }
    if (best) return best;
    /* Known Spreadtrum vendor but unlisted product -> generic. */
    if (vid == 0x1782) return &k_generic;
    return NULL;
}

const chip_profile *chipdb_by_name(const char *name)
{
    int i;
    if (!name) return NULL;
    for (i = 0; i < chipdb_count(); i++) {
        const char *a = k_chips[i].name;
        const char *b = name;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            a++; b++;
        }
        if (*a == '\0' && *b == '\0') return &k_chips[i];
    }
    return NULL;
}

const chip_profile *chipdb_generic(void)
{
    return &k_generic;
}

const char *flash_kind_str(flash_kind k)
{
    switch (k) {
    case FLASH_NAND:     return "NAND";
    case FLASH_EMMC:     return "eMMC";
    case FLASH_NOR:      return "NOR";
    case FLASH_SPI_NAND: return "SPI-NAND";
    default:             return "unknown";
    }
}

const char *capability_str(dump_capability c)
{
    switch (c) {
    case CAP_SUPPORTED:   return "supported";
    case CAP_UNVERIFIED:  return "unverified";
    default:              return "unsupported";
    }
}
