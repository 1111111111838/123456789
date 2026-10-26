/*
 * main.c - Unisoc Dump Tool entry point.
 *
 * GUI application (Win32). The core engine is UI-agnostic, so a headless/CLI
 * front end could be added later without touching the engine modules.
 */
#include "ui/ui_main.h"

#ifdef _WIN32
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdline, int nShow)
{
    (void)hPrev;
    (void)cmdline;
    return ui_run(hInst, nShow);
}

#else

/* On non-Windows the GUI is unavailable; the tests target exercises the core. */
#include <stdio.h>
int main(void)
{
    fprintf(stderr,
        "Unisoc Dump Tool GUI is Windows-only. Build the 'udt_tests' target to "
        "run the portable core tests on this platform.\n");
    return 0;
}

#endif
