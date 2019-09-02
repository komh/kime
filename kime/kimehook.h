// 2001.05.25
#ifndef __KIMEHOOK_H__
#define __KIMEHOOK_H__

#define INCL_PM
#include <os2.h>

#include "kimedlg.h"
#include "khserver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagHOOKDATA
{
    HWND hwndKime;
    HWND hwndHIA;
    HWND hwndIB;
    HWND hwndKHS;
} HOOKDATA, *PHOOKDATA;

extern OPTDLGPARAM kimeOpt;

BOOL EXPENTRY installHook( HAB, PHOOKDATA );
VOID EXPENTRY uninstallHook( VOID );

#ifdef __cplusplus
}
#endif

#endif

