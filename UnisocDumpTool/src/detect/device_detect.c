#include "device_detect.h"

#include <stdio.h>
#include <string.h>

#include "../common/logging.h"
#include "../comm/transport.h"

#ifdef _WIN32
static void parse_vid_pid(const char *hwid, uint16_t *vid, uint16_t *pid, int *ok)
{
    /* hwid looks like "USB\VID_1782&PID_4D00&..." (case-insensitive). */
    const char *p;
    unsigned v = 0, d = 0;
    *ok = 0;
    if (!hwid) return;

    p = strstr(hwid, "VID_");
    if (!p) p = strstr(hwid, "vid_");
    if (p && sscanf(p + 4, "%4x", &v) == 1) {
        const char *q = strstr(hwid, "PID_");
        if (!q) q = strstr(hwid, "pid_");
        if (q && sscanf(q + 4, "%4x", &d) == 1) {
            *vid = (uint16_t)v;
            *pid = (uint16_t)d;
            *ok = 1;
        }
    }
}
#endif /* _WIN32 */

void udt_device_summary(const udt_device_info *info, char *buf, int cap)
{
    if (!info || !info->present) {
        snprintf(buf, cap, "No Unisoc device detected");
        return;
    }
    snprintf(buf, cap, "%s [%s] VID_%04X&PID_%04X%s%s  cap=%s%s",
             info->chip ? info->chip->name : "Unknown",
             info->chip ? info->chip->family : "",
             info->usb_vid, info->usb_pid,
             info->com_port[0] ? " on " : "",
             info->com_port[0] ? info->com_port : "",
             info->chip ? capability_str(info->chip->capability) : "?",
             info->chip_confirmed ? "" : " (inferred)");
}

/* ------------------------------------------------------------------------- */
#ifdef _WIN32

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>

/* GUID_DEVINTERFACE_COMPORT and the Ports class. We query the Ports (COM & LPT)
 * device class so we find the virtual COM the Spreadtrum download driver makes. */
static const GUID k_guid_ports =
    { 0x4d36e978, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };

static int read_com_number(HDEVINFO devs, SP_DEVINFO_DATA *dd, char *out, int cap)
{
    HKEY key = SetupDiOpenDevRegKey(devs, dd, DICS_FLAG_GLOBAL, 0,
                                    DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE) return 0;
    {
        char name[16];
        DWORD type = 0, len = sizeof(name);
        LONG r = RegQueryValueExA(key, "PortName", NULL, &type,
                                  (LPBYTE)name, &len);
        RegCloseKey(key);
        if (r == ERROR_SUCCESS && (type == REG_SZ)) {
            snprintf(out, cap, "%s", name);
            return 1;
        }
    }
    return 0;
}

int udt_detect_once(udt_device_info *out)
{
    HDEVINFO devs;
    SP_DEVINFO_DATA dd;
    DWORD i;
    int found = 0;

    memset(out, 0, sizeof(*out));
    out->chip = NULL;

    devs = SetupDiGetClassDevsA(&k_guid_ports, NULL, NULL,
                                DIGCF_PRESENT);
    if (devs == INVALID_HANDLE_VALUE) {
        LOGE("SetupDiGetClassDevs failed (%lu)", (unsigned long)GetLastError());
        return UDT_ERR_IO;
    }

    dd.cbSize = sizeof(dd);
    for (i = 0; !found && SetupDiEnumDeviceInfo(devs, i, &dd); i++) {
        char hwid[512] = {0};
        char friendly[UDT_NAME_MAX] = {0};
        uint16_t vid = 0, pid = 0;
        int ok = 0;
        DWORD type = 0;

        SetupDiGetDeviceRegistryPropertyA(devs, &dd, SPDRP_HARDWAREID, &type,
                                          (PBYTE)hwid, sizeof(hwid), NULL);
        SetupDiGetDeviceRegistryPropertyA(devs, &dd, SPDRP_FRIENDLYNAME, &type,
                                          (PBYTE)friendly, sizeof(friendly), NULL);

        parse_vid_pid(hwid, &vid, &pid, &ok);
        if (!ok) continue;

        /* Only consider the Spreadtrum vendor id. New generations are added in
         * chipdb; here we gate on the vendor so we do not grab unrelated COM
         * ports. */
        if (vid != 0x1782) continue;

        out->present = 1;
        out->usb_vid = vid;
        out->usb_pid = pid;
        out->usb_ids_confirmed = 1;
        snprintf(out->friendly_name, sizeof(out->friendly_name), "%s", friendly);
        snprintf(out->device_path, sizeof(out->device_path), "%s", hwid);
        read_com_number(devs, &dd, out->com_port, sizeof(out->com_port));

        out->chip = chipdb_by_usb(vid, pid);
        if (!out->chip) out->chip = chipdb_generic();
        /* We matched by USB id from the OS: the port is confirmed, the exact
         * SoC behind it is still inferred unless a later probe confirms it. */
        out->chip_confirmed = 0;

        found = 1;
    }

    SetupDiDestroyDeviceInfoList(devs);
    return found ? 1 : 0;
}

#else /* ---- non-Windows stub, so the core + tests build/run ---- */

int udt_detect_once(udt_device_info *out)
{
    memset(out, 0, sizeof(*out));
    /* No real enumeration off Windows. The simulator provides a device in
     * test/sim mode without going through this path. */
    return 0;
}

#endif
