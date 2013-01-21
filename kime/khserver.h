#ifndef __KHSERVER_H__
#define __KHSERVER_H__

#include <os2.h>

//#define DEBUG

#define WC_KIMEHOOKSERVER       "KIME_HOOK_SERVER"

#define KHSM_FINDWND                ( WM_USER + 1 )
#define KHSM_ADDWND                 ( WM_USER + 2 )
#define KHSM_DELWND                 ( WM_USER + 3 )
#define KHSM_QUERYHANSTATUS         ( WM_USER + 4 )
#define KHSM_CHANGEHANSTATUS        ( WM_USER + 5 )
#define KHSM_SETHANSTATUS           ( WM_USER + 6 )
#define KHSM_QUERYIMSTATUS          ( WM_USER + 7 )
#define KHSM_CHANGEIMSTATUS         ( WM_USER + 8 )
#define KHSM_SETIMSTATUS            ( WM_USER + 9 )
#define KHSM_CHECKDBCSSUPPORT       ( WM_USER + 10 )

#define KHSM_ISEXCEPTWINDOW         ( WM_USER + 11 )

#ifdef DEBUG
#define KHSM_STOREKEYINFO           ( WM_USER + 100 )
#endif

#ifdef __cplusplus
extern "C" {
#endif

BOOL EXPENTRY RegisterKimeHookServer( HAB hab );
BOOL EXPENTRY KHSCreateKimeHookServer( HWND hwndOwner, ULONG id );

#ifdef __cplusplus
}
#endif

#endif
