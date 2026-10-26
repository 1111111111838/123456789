/*
 * ui_main.h - Entry point for the Material-style main window.
 */
#ifndef UDT_UI_MAIN_H
#define UDT_UI_MAIN_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Create the main window and run the message loop. Returns the exit code. */
int ui_run(HINSTANCE hInst, int nCmdShow);

#ifdef __cplusplus
}
#endif

#endif /* UDT_UI_MAIN_H */
