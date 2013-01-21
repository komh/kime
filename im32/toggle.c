#include <os2.h>
#include <os2im.h>
#include "toggle.h"

VOID toggleIMEHanEng( HWND hwnd )
{
    HWND hwndIm;
    HIMI himi;
    ULONG ulInputMode, ulConversionMode;

    ImGetInstance( hwnd, &himi );

    ImQueryIMMode( himi, &ulInputMode, &ulConversionMode );

    ulInputMode ^= IMI_IM_NLS_HANGEUL;

    ImSetIMMode( himi, ulInputMode, ulConversionMode );

    ImReleaseInstance( hwnd, himi );
}

VOID callIMEHanja( HWND hwnd )
{
    HIMI himi;
    ULONG ulInputMode, ulConversionMode;

    ImGetInstance( hwnd, &himi );

    ImQueryIMMode( himi, &ulInputMode, &ulConversionMode );

    ulInputMode |= IMI_IM_IME_ON;

    ImSetIMMode( himi, ulInputMode, ulConversionMode );

    ImReleaseInstance( hwnd, himi );
}

