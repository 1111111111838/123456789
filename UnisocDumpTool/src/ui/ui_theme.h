/*
 * ui_theme.h - Material-inspired palette, fonts and drawing helpers (Win32).
 *
 * The exact palette is mandated by the specification:
 *   primary blue  #2563EB   accent cyan   #7DD3FC
 *   white         #FFFFFF   cream         #FFF8E7
 *   bg secondary  #F1F7FB   dark text     #243247
 */
#ifndef UDT_UI_THEME_H
#define UDT_UI_THEME_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLR_PRIMARY   RGB(0x25, 0x63, 0xEB)
#define CLR_PRIMARY_D RGB(0x1E, 0x4F, 0xC0)   /* pressed */
#define CLR_ACCENT    RGB(0x7D, 0xD3, 0xFC)
#define CLR_WHITE     RGB(0xFF, 0xFF, 0xFF)
#define CLR_CREAM     RGB(0xFF, 0xF8, 0xE7)
#define CLR_BG        RGB(0xF1, 0xF7, 0xFB)
#define CLR_TEXT      RGB(0x24, 0x32, 0x47)
#define CLR_TEXT_MUT  RGB(0x6B, 0x7A, 0x8D)
#define CLR_OK        RGB(0x1F, 0x9D, 0x55)
#define CLR_WARN      RGB(0xC9, 0x7A, 0x12)
#define CLR_ERR       RGB(0xC6, 0x28, 0x28)
#define CLR_BORDER    RGB(0xDD, 0xE7, 0xF0)

typedef struct {
    HFONT h1;        /* card titles */
    HFONT body;      /* normal text */
    HFONT small;     /* captions / log */
    HFONT button;    /* button labels */
    HBRUSH bg;
    HBRUSH white;
    HBRUSH cream;
} ui_theme;

void ui_theme_init(ui_theme *t);
void ui_theme_free(ui_theme *t);

/* Fill a rounded rectangle with a 1px border. */
void ui_fill_round(HDC dc, const RECT *r, int radius, COLORREF fill, COLORREF border);

/* Draw a soft drop shadow behind a card rect (cheap, no gradients). */
void ui_card_shadow(HDC dc, const RECT *r, int radius);

/* Owner-draw a button given its DRAWITEMSTRUCT. primary=filled blue,
 * otherwise outlined. */
void ui_draw_button(const DRAWITEMSTRUCT *dis, const ui_theme *t,
                    const wchar_t *text, int primary, int enabled);

/* Owner-draw a cyan progress bar. pct 0..100, or -1 for indeterminate stripes. */
void ui_draw_progress(HDC dc, const RECT *r, int pct);

/* Status dot: green/amber/red filled circle. */
void ui_status_dot(HDC dc, int cx, int cy, int radius, COLORREF color);

#ifdef __cplusplus
}
#endif

#endif /* UDT_UI_THEME_H */
