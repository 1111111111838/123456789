/* resource.h - control and resource identifiers. */
#ifndef UDT_RESOURCE_H
#define UDT_RESOURCE_H

#define IDI_APPICON        101
#define IDR_MANIFEST         1

/* Controls on the main window. */
#define IDC_BTN_START     1001
#define IDC_BTN_CANCEL    1002
#define IDC_BTN_BROWSE    1003
#define IDC_BTN_DETAILS   1004
#define IDC_EDIT_PATH     1005
#define IDC_LIST_LOG      1006
#define IDC_CHK_DETAILS   1007   /* "show technical details" toggle */
#define IDC_BTN_SIM       1008   /* run a simulated dump (test mode) */
#define IDC_BTN_FDL1      1009   /* choose FDL1 image (optional) */
#define IDC_BTN_FDL2      1010   /* choose FDL2 image (optional) */
#define IDC_BTN_FDLCLR    1011   /* clear chosen FDL images */
#define IDC_PROMPT_EDIT   1050   /* edit control inside the address prompt */

/* Custom window messages (worker thread -> UI thread). */
#define WM_APP_LOG       (WM_APP + 1)   /* wParam=level, lParam=heap wchar_t* */
#define WM_APP_PROGRESS  (WM_APP + 2)   /* lParam = heap dump_progress copy    */
#define WM_APP_DONE      (WM_APP + 3)   /* wParam = complete(1)/partial(0)/err */
#define WM_APP_DETECT    (WM_APP + 4)   /* device detection state changed      */

#endif /* UDT_RESOURCE_H */
