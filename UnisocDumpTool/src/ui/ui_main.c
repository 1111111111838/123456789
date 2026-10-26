#include "ui_main.h"
#include "ui_theme.h"
#include "resource.h"

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/logging.h"
#include "../common/appconfig.h"
#include "../comm/transport.h"
#include "../comm/serial.h"
#include "../sim/sim_device.h"
#include "../detect/device_detect.h"
#include "../flash/flash_dump.h"
#include "../fileout/dump_writer.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define WIN_W 920
#define WIN_H 660
#define PAD   20

typedef struct {
    HWND hwnd;
    HWND edit_path, btn_browse, btn_start, btn_cancel, btn_details, btn_sim;
    HWND list_log;
    ui_theme theme;

    udt_device_info dev;
    int dev_present;
    int confirm_shown;        /* avoid re-prompting for the same device */

    wchar_t out_dir[MAX_PATH];
    app_config cfg;

    HANDLE worker;
    volatile int cancel;
    volatile int running;
    int sim_run;              /* 1 if the active run is simulated */

    int   prog_pct;
    double prog_speed;
    unsigned long long prog_done, prog_total;
} app_state;

static app_state g_app;

/* ---- log sink: marshal worker-thread log lines onto the UI thread ---- */
static void ui_log_sink(void *user, log_level level, const char *msg)
{
    app_state *a = (app_state *)user;
    int wn;
    wchar_t *copy;
    if (!a || !a->hwnd) return;
    wn = MultiByteToWideChar(CP_UTF8, 0, msg, -1, NULL, 0);
    if (wn <= 0) return;
    copy = (wchar_t *)malloc(sizeof(wchar_t) * wn);
    if (!copy) return;
    MultiByteToWideChar(CP_UTF8, 0, msg, -1, copy, wn);
    PostMessageW(a->hwnd, WM_APP_LOG, (WPARAM)level, (LPARAM)copy);
}

static void log_add(app_state *a, log_level level, const wchar_t *text)
{
    int idx = (int)SendMessageW(a->list_log, LB_ADDSTRING, 0, (LPARAM)text);
    if (idx != LB_ERR) {
        SendMessageW(a->list_log, LB_SETITEMDATA, idx, (LPARAM)level);
        SendMessageW(a->list_log, LB_SETTOPINDEX, idx, 0);
    }
}

/* ---- target folder ---- */
static void set_default_outdir(app_state *a)
{
    wchar_t docs[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, docs))) {
        _snwprintf_s(a->out_dir, MAX_PATH, _TRUNCATE, L"%s\\UnisocDumps", docs);
    } else {
        wcscpy_s(a->out_dir, MAX_PATH, L"C:\\UnisocDumps");
    }
    SetWindowTextW(a->edit_path, a->out_dir);
}

static int browse_folder(app_state *a)
{
    BROWSEINFOW bi;
    LPITEMIDLIST pidl;
    wchar_t path[MAX_PATH];
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = a->hwnd;
    bi.lpszTitle = L"בחר תיקיית יעד לשמירת ה-Dump";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return 0;
    if (SHGetPathFromIDListW(pidl, path)) {
        wcscpy_s(a->out_dir, MAX_PATH, path);
        SetWindowTextW(a->edit_path, a->out_dir);
    }
    CoTaskMemFree(pidl);
    return 1;
}

static unsigned long long free_space_bytes(const wchar_t *dir)
{
    ULARGE_INTEGER freeb;
    wchar_t root[MAX_PATH];
    wcscpy_s(root, MAX_PATH, dir);
    /* GetDiskFreeSpaceEx accepts a directory; use the first existing ancestor. */
    if (GetDiskFreeSpaceExW(root, &freeb, NULL, NULL)) return freeb.QuadPart;
    /* fall back to drive root */
    if (wcslen(root) >= 3) { root[3] = L'\0';
        if (GetDiskFreeSpaceExW(root, &freeb, NULL, NULL)) return freeb.QuadPart; }
    return 0;
}

/* ---- confirmation dialog ---- */
static int show_confirm_dialog(app_state *a)
{
    /* Simple modal using a message box styled with device facts. A full custom
     * Material dialog template is in docs/UI.md; this keeps the confirmation
     * unmistakable and native. */
    wchar_t text[1024];
    char summary[512];
    wchar_t wsummary[512];
    const chip_profile *chip = a->dev.chip;

    udt_device_summary(&a->dev, summary, (int)sizeof(summary));
    MultiByteToWideChar(CP_UTF8, 0, summary, -1, wsummary, 512);

    _snwprintf_s(text, 1024, _TRUNCATE,
        L"נמצא מכשיר Unisoc\n\n"
        L"דגם: %hs\n"
        L"שבב: %hs%s\n"
        L"ממשק תקשורת: %hs\n"
        L"מצב חיבור: מחובר\n"
        L"יכולת שאיבה: %hs\n\n"
        L"להתחיל שאיבת Dump?\n\n"
        L"(שים לב: שאיבה אמיתית דורשת פרמטרים תקינים ב-UnisocDumpTool.ini, "
        L"ולרוב קובצי FDL מתאימים לשבב.)",
        chip ? chip->name : "Unknown",
        chip ? chip->family : "",
        a->dev.chip_confirmed ? L"" : L" (משוער)",
        a->dev.com_port[0] ? a->dev.com_port : "USB",
        chip ? capability_str(chip->capability) : "unknown");

    int r = MessageBoxW(a->hwnd, text, L"אישור שאיבה",
                        MB_YESNO | MB_ICONINFORMATION);
    return r == IDYES;
}

/* ---- worker thread ---- */
typedef struct {
    app_state *a;
    int simulate;
} worker_args;

static int worker_progress(void *user, const dump_progress *p)
{
    app_state *a = (app_state *)user;
    dump_progress *copy = (dump_progress *)malloc(sizeof(*copy));
    if (copy) { *copy = *p; PostMessageW(a->hwnd, WM_APP_PROGRESS, 0, (LPARAM)copy); }
    return a->cancel;   /* non-zero -> engine cancels */
}

static DWORD WINAPI worker_main(LPVOID param)
{
    worker_args *wa = (worker_args *)param;
    app_state *a = wa->a;
    int simulate = wa->simulate;
    free(wa);

    udt_transport *t = NULL;
    dump_writer *w = NULL;
    dump_params p;
    udt_status st;
    int complete = 0;
    char out_dir_utf8[MAX_PATH * 3];
    char label[64] = "Device";

    WideCharToMultiByte(CP_UTF8, 0, a->out_dir, -1, out_dir_utf8,
                        sizeof(out_dir_utf8), NULL, NULL);

    /* Open transport. */
    if (simulate) {
        sim_config sc = {0};
        sc.chip_name = "SIM-SC6531E";
        sc.flash_size = 8 * 1024 * 1024;
        sc.require_fdl = 0;
        st = udt_sim_open(&sc, &t);
        strcpy_s(label, sizeof(label), "SIM-SC6531E");
    } else {
        if (!a->dev_present || !a->dev.com_port[0]) {
            LOGE("No real device/port available. Connect a device in download "
                 "mode, or use Simulated Dump for a test run.");
            PostMessageW(a->hwnd, WM_APP_DONE, (WPARAM)-1, 0);
            a->running = 0;
            return 0;
        }
        st = udt_serial_open(a->dev.com_port, a->cfg.baud ? a->cfg.baud : 115200, &t);
        if (a->dev.chip && a->dev.chip->name)
            strncpy_s(label, sizeof(label), a->dev.chip->name, _TRUNCATE);
    }
    if (st != UDT_OK || !t) {
        LOGE("Failed to open transport: %s", udt_status_str(st));
        PostMessageW(a->hwnd, WM_APP_DONE, (WPARAM)-1, 0);
        a->running = 0;
        return 0;
    }

    /* Open output. */
    st = dump_open(&w, out_dir_utf8, label, "flash_dump.bin", simulate);
    if (st != UDT_OK) {
        LOGE("Failed to create output: %s", udt_status_str(st));
        t->close(t);
        PostMessageW(a->hwnd, WM_APP_DONE, (WPARAM)-1, 0);
        a->running = 0;
        return 0;
    }

    /* Write device_info.txt before reading. */
    {
        char info[1024];
        snprintf(info, sizeof(info),
            "Unisoc Dump Tool - device info\n"
            "mode: %s\n"
            "chip: %s\n"
            "family: %s\n"
            "usb: VID_%04X&PID_%04X\n"
            "port: %s\n"
            "capability: %s\n",
            simulate ? "SIMULATED" : "REAL",
            (a->dev.chip ? a->dev.chip->name : (simulate ? "SIM-SC6531E" : "Unknown")),
            (a->dev.chip ? a->dev.chip->family : ""),
            a->dev.usb_vid, a->dev.usb_pid,
            (a->dev.com_port[0] ? a->dev.com_port : (simulate ? "SIM" : "?")),
            (a->dev.chip ? capability_str(a->dev.chip->capability) : "unknown"));
        dump_write_text(w, "device_info.txt", info);
    }

    /* Build params. */
    memset(&p, 0, sizeof(p));
    p.cancel = &a->cancel;
    if (simulate) {
        p.method = READ_LINEAR;
        p.base_addr = 0;
        p.total_size = 8 * 1024 * 1024;
        p.chunk_size = 2048;
    } else {
        p.method = (read_method)a->cfg.method;
        p.base_addr = a->cfg.base_addr;
        p.total_size = a->cfg.flash_size;        /* 0 -> engine refuses, honestly */
        p.chunk_size = a->cfg.chunk;
        p.baud = a->cfg.baud;
        p.fdl1_path = a->cfg.fdl1[0] ? a->cfg.fdl1 : NULL;
        p.fdl1_addr = a->cfg.fdl1_addr;
        p.fdl2_path = a->cfg.fdl2[0] ? a->cfg.fdl2 : NULL;
        p.fdl2_addr = a->cfg.fdl2_addr;
        if (!a->cfg.have_size) {
            LOGE("Real dump needs 'size' in UnisocDumpTool.ini (the flash size "
                 "from the device PAC). Without it the dump has no defined "
                 "extent. See docs/MISSING.md.");
        }
    }

    st = flash_dump_run(t, w, &p, worker_progress, a, &complete);

    dump_close(w);
    t->close(t);

    PostMessageW(a->hwnd, WM_APP_DONE,
                 (WPARAM)(st != UDT_OK ? -1 : (complete ? 1 : 0)), 0);
    a->running = 0;
    return 0;
}

static void start_dump(app_state *a, int simulate)
{
    worker_args *wa;
    if (a->running) return;

    if (!simulate) {
        if (!show_confirm_dialog(a)) return;
    }

    a->cancel = 0;
    a->running = 1;
    a->sim_run = simulate;
    a->prog_pct = 0; a->prog_done = 0; a->prog_total = 0; a->prog_speed = 0;
    EnableWindow(a->btn_start, FALSE);
    EnableWindow(a->btn_sim, FALSE);
    EnableWindow(a->btn_cancel, TRUE);
    InvalidateRect(a->hwnd, NULL, FALSE);

    wa = (worker_args *)malloc(sizeof(*wa));
    wa->a = a; wa->simulate = simulate;
    a->worker = CreateThread(NULL, 0, worker_main, wa, 0, NULL);
    if (!a->worker) {
        a->running = 0;
        free(wa);
        LOGE("Could not start worker thread");
    }
}

/* ---- painting ---- */
static void paint_card(HDC dc, app_state *a, RECT r, const wchar_t *title)
{
    ui_card_shadow(dc, &r, 12);
    ui_fill_round(dc, &r, 12, CLR_WHITE, CLR_BORDER);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, a->theme.h1);
    SetTextColor(dc, CLR_TEXT);
    {
        RECT tr = r;
        tr.left += 18; tr.top += 12; tr.right -= 18;
        DrawTextW(dc, title, -1, &tr, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
}

static void text_at(HDC dc, HFONT f, COLORREF c, int x, int y, const wchar_t *s)
{
    SelectObject(dc, f);
    SetTextColor(dc, c);
    TextOutW(dc, x, y, s, (int)wcslen(s));
}

static void on_paint(app_state *a)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(a->hwnd, &ps);
    RECT cr; GetClientRect(a->hwnd, &cr);

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
    HBITMAP ob = (HBITMAP)SelectObject(mem, bmp);

    /* background */
    FillRect(mem, &cr, a->theme.bg);

    /* top bar */
    RECT top = { 0, 0, cr.right, 56 };
    HBRUSH topb = CreateSolidBrush(CLR_PRIMARY);
    FillRect(mem, &top, topb);
    DeleteObject(topb);
    text_at(mem, a->theme.h1, CLR_WHITE, PAD, 16, L"Unisoc Dump Tool");
    {
        COLORREF dot = a->dev_present ? CLR_OK : RGB(0xB7, 0xC6, 0xE8);
        ui_status_dot(mem, cr.right - 150, 28, 6, dot);
        text_at(mem, a->theme.small, CLR_WHITE, cr.right - 135, 20,
                a->dev_present ? L"מחובר" : L"ממתין לחיבור");
    }

    int x = PAD, y = 56 + PAD;
    int cardw = cr.right - 2 * PAD;

    /* Device card */
    RECT dcard = { x, y, x + cardw, y + 110 };
    paint_card(mem, a, dcard, L"מכשיר");
    {
        const chip_profile *chip = a->dev.chip;
        wchar_t line[256];
        int ty = dcard.top + 44;
        if (a->dev_present && chip) {
            _snwprintf_s(line, 256, _TRUNCATE, L"%hs  •  %hs", chip->name, chip->family);
            text_at(mem, a->theme.body, CLR_TEXT, dcard.left + 18, ty, line);
            _snwprintf_s(line, 256, _TRUNCATE,
                L"ממשק: %hs    יכולת שאיבה: %hs%s",
                a->dev.com_port[0] ? a->dev.com_port : "USB",
                capability_str(chip->capability),
                a->dev.chip_confirmed ? L"" : L" (משוער)");
            text_at(mem, a->theme.small, CLR_TEXT_MUT, dcard.left + 18, ty + 24, line);
            ui_status_dot(mem, dcard.right - 24, dcard.top + 24, 6, CLR_OK);
        } else {
            text_at(mem, a->theme.body, CLR_TEXT_MUT, dcard.left + 18, ty,
                    L"לא זוהה מכשיר. חבר מכשיר Unisoc במצב Download.");
        }
    }
    y = dcard.bottom + PAD;

    /* Target card (controls positioned in WM_SIZE/create; here draw labels) */
    RECT tcard = { x, y, x + cardw, y + 110 };
    paint_card(mem, a, tcard, L"יעד");
    {
        unsigned long long fb = free_space_bytes(a->out_dir);
        wchar_t line[128];
        _snwprintf_s(line, 128, _TRUNCATE, L"נפח פנוי בדיסק: %.1f GB",
                     (double)fb / (1024.0 * 1024.0 * 1024.0));
        text_at(mem, a->theme.small, CLR_TEXT_MUT, tcard.left + 18, tcard.bottom - 30, line);
    }
    y = tcard.bottom + PAD;

    /* Dump card */
    RECT pcard = { x, y, x + cardw, y + 120 };
    paint_card(mem, a, pcard, L"שאיבה");
    {
        RECT bar = { pcard.left + 18, pcard.top + 52, pcard.right - 18, pcard.top + 70 };
        ui_draw_progress(mem, &bar, a->running || a->prog_pct ? a->prog_pct : 0);
        wchar_t line[256];
        if (a->prog_total) {
            _snwprintf_s(line, 256, _TRUNCATE,
                L"%d%%   •   %.2f / %.2f MB   •   %.0f KB/s%s",
                a->prog_pct,
                (double)a->prog_done / (1024.0 * 1024.0),
                (double)a->prog_total / (1024.0 * 1024.0),
                a->prog_speed / 1024.0,
                a->sim_run ? L"   (סימולציה)" : L"");
        } else if (a->running) {
            wcscpy_s(line, 256, L"מתחיל…");
        } else {
            wcscpy_s(line, 256, L"מוכן");
        }
        text_at(mem, a->theme.small, CLR_TEXT_MUT, pcard.left + 18, pcard.top + 80, line);
    }
    y = pcard.bottom + PAD;

    /* Log card frame (listbox is a child drawn on top) */
    RECT lcard = { x, y, x + cardw, cr.bottom - PAD };
    paint_card(mem, a, lcard, L"יומן");

    BitBlt(dc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);

    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(a->hwnd, &ps);
}

static void layout_children(app_state *a)
{
    RECT cr; GetClientRect(a->hwnd, &cr);
    int x = PAD;
    int cardw = cr.right - 2 * PAD;

    int y_target = 56 + PAD + 110 + PAD;      /* top + device card */
    int y_dump   = y_target + 110 + PAD;
    int y_log    = y_dump + 120 + PAD;

    /* Target card controls */
    MoveWindow(a->edit_path, x + 18, y_target + 44, cardw - 160, 28, TRUE);
    MoveWindow(a->btn_browse, x + cardw - 130, y_target + 42, 112, 32, TRUE);

    /* Dump card buttons (right aligned in header area) */
    MoveWindow(a->btn_start,  x + cardw - 150, y_dump + 12, 132, 36, TRUE);
    MoveWindow(a->btn_cancel, x + cardw - 150 - 120, y_dump + 12, 108, 36, TRUE);
    MoveWindow(a->btn_sim,    x + 18,              y_dump + 12, 150, 36, TRUE);

    /* Log listbox fills the log card */
    MoveWindow(a->list_log, x + 14, y_log + 40, cardw - 28,
               (cr.bottom - PAD) - (y_log + 48), TRUE);

    /* Details toggle bottom-right */
    MoveWindow(a->btn_details, x + cardw - 150, y_log + 6, 132, 28, TRUE);
}

static void create_children(app_state *a, HINSTANCE hInst)
{
    a->edit_path = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
        0, 0, 10, 10, a->hwnd, (HMENU)IDC_EDIT_PATH, hInst, NULL);
    SendMessageW(a->edit_path, WM_SETFONT, (WPARAM)a->theme.body, TRUE);

    a->btn_browse = CreateWindowW(L"BUTTON", L"עיון…",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, a->hwnd, (HMENU)IDC_BTN_BROWSE, hInst, NULL);
    a->btn_start = CreateWindowW(L"BUTTON", L"התחל שאיבה",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, a->hwnd, (HMENU)IDC_BTN_START, hInst, NULL);
    a->btn_cancel = CreateWindowW(L"BUTTON", L"ביטול",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_DISABLED,
        0, 0, 10, 10, a->hwnd, (HMENU)IDC_BTN_CANCEL, hInst, NULL);
    a->btn_sim = CreateWindowW(L"BUTTON", L"שאיבת סימולציה",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, a->hwnd, (HMENU)IDC_BTN_SIM, hInst, NULL);
    a->btn_details = CreateWindowW(L"BUTTON", L"פרטים טכניים",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 10, 10, a->hwnd, (HMENU)IDC_BTN_DETAILS, hInst, NULL);

    a->list_log = CreateWindowExW(0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT |
        LBS_OWNERDRAWFIXED | LBS_NOSEL,
        0, 0, 10, 10, a->hwnd, (HMENU)IDC_LIST_LOG, hInst, NULL);
    SendMessageW(a->list_log, WM_SETFONT, (WPARAM)a->theme.small, TRUE);
    SendMessageW(a->list_log, LB_SETITEMHEIGHT, 0, (LPARAM)18);
}

/* ---- window proc ---- */
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    app_state *a = &g_app;

    switch (m) {
    case WM_CREATE: {
        HINSTANCE hInst = ((LPCREATESTRUCTW)lp)->hInstance;
        a->hwnd = h;
        ui_theme_init(&a->theme);
        create_children(a, hInst);
        set_default_outdir(a);

        /* Load real-dump parameters (honest: missing values -> clear errors). */
        {
            char ini[MAX_PATH];
            appconfig_default_path(ini, sizeof(ini));
            appconfig_load(ini, &a->cfg);
        }

        log_set_sink(ui_log_sink, a);
        LOGI("Unisoc Dump Tool started. Connect a device in download mode.");
        LOGI("Real reads need valid UnisocDumpTool.ini parameters (and usually "
             "an FDL). Use 'Simulated Dump' to exercise the full pipeline.");

        layout_children(a);
        SetTimer(h, 1, 800, NULL);   /* device detection poll */
        return 0;
    }

    case WM_SIZE:
        layout_children(a);
        InvalidateRect(h, NULL, FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        mmi->ptMinTrackSize.x = 760;
        mmi->ptMinTrackSize.y = 560;
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;   /* handled in WM_PAINT with double buffering */

    case WM_PAINT:
        on_paint(a);
        return 0;

    case WM_TIMER: {
        udt_device_info info;
        int found = udt_detect_once(&info);
        int was = a->dev_present;
        if (found == 1) {
            if (!was || memcmp(&info.usb_pid, &a->dev.usb_pid, sizeof(info.usb_pid)) != 0
                     || strcmp(info.com_port, a->dev.com_port) != 0) {
                a->dev = info;
                a->dev_present = 1;
                InvalidateRect(h, NULL, FALSE);
                if (!a->confirm_shown && !a->running) {
                    a->confirm_shown = 1;
                    char s[256]; udt_device_summary(&info, s, sizeof(s));
                    LOGI("Detected: %s", s);
                }
            }
        } else {
            if (was) {
                a->dev_present = 0;
                a->confirm_shown = 0;
                memset(&a->dev, 0, sizeof(a->dev));
                InvalidateRect(h, NULL, FALSE);
                LOGW("Device disconnected.");
            }
        }
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (dis->CtlType == ODT_BUTTON) {
            const wchar_t *label = L"";
            int primary = 0, enabled = IsWindowEnabled(dis->hwndItem);
            switch (dis->CtlID) {
            case IDC_BTN_START:  label = L"התחל שאיבה"; primary = 1; break;
            case IDC_BTN_CANCEL: label = L"ביטול"; break;
            case IDC_BTN_BROWSE: label = L"עיון…"; break;
            case IDC_BTN_SIM:    label = L"שאיבת סימולציה"; break;
            case IDC_BTN_DETAILS:label = L"פרטים טכניים"; break;
            }
            ui_draw_button(dis, &a->theme, label, primary, enabled);
            return TRUE;
        }
        if (dis->CtlType == ODT_LISTBOX && (int)dis->itemID >= 0) {
            wchar_t buf[1024];
            log_level lvl = (log_level)SendMessageW(dis->hwndItem, LB_GETITEMDATA,
                                                    dis->itemID, 0);
            COLORREF c = CLR_TEXT;
            if (lvl == LOG_WARN) c = CLR_WARN;
            else if (lvl == LOG_ERROR) c = CLR_ERR;
            else if (lvl == LOG_DEBUG) c = CLR_TEXT_MUT;
            SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)buf);
            FillRect(dis->hDC, &dis->rcItem, a->theme.white);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, c);
            SelectObject(dis->hDC, a->theme.small);
            {
                RECT tr = dis->rcItem; tr.left += 6;
                DrawTextW(dis->hDC, buf, -1, &tr,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            return TRUE;
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_BROWSE:  browse_folder(a); InvalidateRect(h, NULL, FALSE); break;
        case IDC_BTN_START:   start_dump(a, 0); break;
        case IDC_BTN_SIM:     start_dump(a, 1); break;
        case IDC_BTN_CANCEL:
            if (a->running) { a->cancel = 1; LOGW("Cancellation requested…"); }
            break;
        case IDC_BTN_DETAILS: {
            log_level cur = log_get_min_level();
            log_set_min_level(cur == LOG_DEBUG ? LOG_INFO : LOG_DEBUG);
            LOGI("Technical details: %s",
                 log_get_min_level() == LOG_DEBUG ? "ON" : "OFF");
            break;
        }
        }
        return 0;

    case WM_APP_LOG: {
        wchar_t *txt = (wchar_t *)lp;
        if (txt) { log_add(a, (log_level)wp, txt); free(txt); }
        return 0;
    }

    case WM_APP_PROGRESS: {
        dump_progress *p = (dump_progress *)lp;
        if (p) {
            a->prog_pct = p->percent >= 0 ? p->percent : a->prog_pct;
            a->prog_done = p->bytes_done;
            a->prog_total = p->bytes_total;
            a->prog_speed = p->speed_bps;
            free(p);
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    }

    case WM_APP_DONE: {
        int code = (int)(intptr_t)wp;
        EnableWindow(a->btn_start, TRUE);
        EnableWindow(a->btn_sim, TRUE);
        EnableWindow(a->btn_cancel, FALSE);
        if (a->worker) { WaitForSingleObject(a->worker, 2000);
            CloseHandle(a->worker); a->worker = NULL; }
        if (code == 1)      LOGI("✔ Dump finished and COMPLETE.");
        else if (code == 0) LOGW("△ Dump finished but INCOMPLETE (see manifest).");
        else                LOGE("✘ Dump failed. See messages above.");
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }

    case WM_CLOSE:
        if (a->running) {
            if (MessageBoxW(h, L"שאיבה פעילה. לבטל ולצאת?", L"יציאה",
                            MB_YESNO | MB_ICONWARNING) != IDYES)
                return 0;
            a->cancel = 1;
            if (a->worker) WaitForSingleObject(a->worker, 3000);
        }
        KillTimer(h, 1);
        DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        log_set_sink(NULL, NULL);
        ui_theme_free(&a->theme);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int ui_run(HINSTANCE hInst, int nCmdShow)
{
    WNDCLASSEXW wc;
    HWND hwnd;
    MSG msg;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };

    InitCommonControlsEx(&icc);
    memset(&g_app, 0, sizeof(g_app));

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"UnisocDumpToolMain";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) return 1;

    hwnd = CreateWindowExW(0, wc.lpszClassName, L"Unisoc Dump Tool",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, WIN_W, WIN_H,
        NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
