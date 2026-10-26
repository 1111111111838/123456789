#include "ui_theme.h"

static HFONT make_font(int pt, int weight)
{
    /* Segoe UI is the standard modern Windows UI font. Negative height = points. */
    int h = -MulDiv(pt, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
    return CreateFontW(h, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       L"Segoe UI");
}

void ui_theme_init(ui_theme *t)
{
    t->h1     = make_font(13, FW_SEMIBOLD);
    t->body   = make_font(10, FW_NORMAL);
    t->small  = make_font(9,  FW_NORMAL);
    t->button = make_font(10, FW_SEMIBOLD);
    t->bg     = CreateSolidBrush(CLR_BG);
    t->white  = CreateSolidBrush(CLR_WHITE);
    t->cream  = CreateSolidBrush(CLR_CREAM);
}

void ui_theme_free(ui_theme *t)
{
    if (t->h1) DeleteObject(t->h1);
    if (t->body) DeleteObject(t->body);
    if (t->small) DeleteObject(t->small);
    if (t->button) DeleteObject(t->button);
    if (t->bg) DeleteObject(t->bg);
    if (t->white) DeleteObject(t->white);
    if (t->cream) DeleteObject(t->cream);
}

void ui_fill_round(HDC dc, const RECT *r, int radius, COLORREF fill, COLORREF border)
{
    HBRUSH b = CreateSolidBrush(fill);
    HPEN   p = CreatePen(PS_SOLID, 1, border);
    HBRUSH ob = (HBRUSH)SelectObject(dc, b);
    HPEN   op = (HPEN)SelectObject(dc, p);
    RoundRect(dc, r->left, r->top, r->right, r->bottom, radius * 2, radius * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

void ui_card_shadow(HDC dc, const RECT *r, int radius)
{
    /* A couple of translucent-looking offset rects approximate a soft shadow
     * without alpha blending dependencies. */
    int i;
    for (i = 3; i >= 1; i--) {
        RECT s = *r;
        int shade = 0xE6 - i * 6;
        HBRUSH b = CreateSolidBrush(RGB(shade, shade + 4, shade + 8));
        HPEN   pen = CreatePen(PS_SOLID, 1, RGB(shade, shade + 4, shade + 8));
        HBRUSH ob; HPEN op;
        OffsetRect(&s, 0, i);
        InflateRect(&s, -i + 2, 0);
        ob = (HBRUSH)SelectObject(dc, b);
        op = (HPEN)SelectObject(dc, pen);
        RoundRect(dc, s.left, s.top, s.right, s.bottom, radius * 2, radius * 2);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(b);
        DeleteObject(pen);
    }
}

void ui_draw_button(const DRAWITEMSTRUCT *dis, const ui_theme *t,
                    const wchar_t *text, int primary, int enabled)
{
    RECT r = dis->rcItem;
    BOOL pressed = (dis->itemState & ODS_SELECTED) ? TRUE : FALSE;
    COLORREF fill, txt, border;

    if (primary) {
        fill = !enabled ? RGB(0xA9, 0xC2, 0xF5)
                        : (pressed ? CLR_PRIMARY_D : CLR_PRIMARY);
        txt = CLR_WHITE;
        border = fill;
    } else {
        fill = pressed ? CLR_BG : CLR_WHITE;
        txt = enabled ? CLR_PRIMARY : CLR_TEXT_MUT;
        border = enabled ? CLR_PRIMARY : CLR_BORDER;
    }

    ui_fill_round(dis->hDC, &r, 8, fill, border);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, txt);
    SelectObject(dis->hDC, t->button);
    DrawTextW(dis->hDC, text, -1, &r,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (dis->itemState & ODS_FOCUS) {
        RECT f = r;
        InflateRect(&f, -3, -3);
        SetTextColor(dis->hDC, txt);
    }
}

void ui_draw_progress(HDC dc, const RECT *r, int pct)
{
    RECT track = *r;
    ui_fill_round(dc, &track, (r->bottom - r->top) / 2, CLR_BG, CLR_BORDER);

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    if (pct > 0) {
        RECT fillr = track;
        int w = (int)((long long)(track.right - track.left) * pct / 100);
        fillr.right = fillr.left + w;
        if (w > 2) {
            ui_fill_round(dc, &fillr, (r->bottom - r->top) / 2, CLR_ACCENT, CLR_ACCENT);
        }
    }
}

void ui_status_dot(HDC dc, int cx, int cy, int radius, COLORREF color)
{
    HBRUSH b = CreateSolidBrush(color);
    HPEN   p = CreatePen(PS_SOLID, 1, color);
    HBRUSH ob = (HBRUSH)SelectObject(dc, b);
    HPEN   op = (HPEN)SelectObject(dc, p);
    Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}
