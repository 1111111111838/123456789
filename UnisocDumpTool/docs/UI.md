# UI design notes (Material-inspired, Win32)

The interface is drawn with the Win32 API using custom (owner) drawing rather
than the classic grey Windows look or the old Android "Holo" look. It aims for a
calm, uncluttered Material Design 3 feel.

## Palette (mandatory)

| token | hex | use |
|-------|-----|-----|
| primary blue | `#2563EB` | top bar, primary button, accents |
| accent cyan  | `#7DD3FC` | progress bar fill |
| white        | `#FFFFFF` | card surfaces |
| cream        | `#FFF8E7` | subtle secondary surface |
| bg secondary | `#F1F7FB` | window background |
| dark text    | `#243247` | primary text |

Defined once in `src/ui/ui_theme.h`.

## Structure

* **Top bar** — product name, connection indicator (green dot + text).
* **Device card** — chip name/family, interface, dump capability (with a
  "(inferred)" tag when the SoC is only guessed), green dot when ready.
* **Target card** — path field, Browse button, free disk space.
* **Dump card** — primary *Start Dump* (blue), secondary *Cancel*, a cyan
  progress bar, and a line with percent / MB read / speed; a *Simulated Dump*
  button for the test path.
* **Log pane** — a listbox drawn per-line, colored by level (info = dark,
  warning = amber, error = red, debug = muted). A *Technical details* toggle
  switches debug lines on/off.

## Drawing rules applied

* Moderately rounded corners (`RoundRect`, radius ~12 for cards, ~8 for
  buttons).
* Generous padding (`PAD = 20`) and a clear text hierarchy (three font sizes).
* Soft, cheap drop shadow behind cards (no heavy effects).
* Double-buffered `WM_PAINT` to avoid flicker.
* Common Controls v6 + per-monitor DPI awareness via the manifest.

## Threading

The dump runs on a worker thread. It communicates with the UI only by
`PostMessage`:

* `WM_APP_LOG` — a heap `wchar_t*` log line (freed by the UI).
* `WM_APP_PROGRESS` — a heap `dump_progress` copy (freed by the UI).
* `WM_APP_DONE` — completion code (complete / incomplete / error).

Cancellation is a single `volatile int` flag the engine polls.
