#ifndef __INPUTBOX_H__
#define __INPUTBOX_H__

#include <os2.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WC_INPUTBOX     "KIME_INPUTBOX_CONTROL"

#define IBM_SETHANCHAR          ( WM_USER + 1 )
#define IBM_QUERYHANCHAR        ( WM_USER + 2 )
#define IBM_SHOWINPUTBOX        ( WM_USER + 3 )

#define IB_BORDER_SIZE  1
#define IB_CHAR_WIDTH  16
#define IB_CHAR_HEIGHT 16

BOOL EXPENTRY RegisterInputBoxControl( HAB hab );
HWND EXPENTRY IBCreateInputBox( HWND hwndOwner, ULONG id );

#ifdef __cplusplus
}
#endif

#endif

