#define INCL_DOSMEMMGR
#define INCL_PM
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>

#include "inputbox.h"

#include "../hanlib/han.h"

typedef struct tagINPUTBOXCTLDATA
{
    HANCHAR hch;
    PRECTL  pCursorPos;
} INPUTBOXCTLDATA, *PINPUTBOXCTLDATA;

static MRESULT ib_wmCreate( HWND, MPARAM, MPARAM );
static MRESULT ib_wmDestroy( HWND, MPARAM, MPARAM );
static MRESULT ib_wmPaint( HWND, MPARAM, MPARAM );

static MRESULT ib_umSetHanChar( HWND, MPARAM, MPARAM );
static MRESULT ib_umQueryHanChar( HWND, MPARAM, MPARAM );
static MRESULT ib_umShowInputBox( HWND, MPARAM, MPARAM );

static MRESULT EXPENTRY ib_wndProc( HWND, ULONG, MPARAM, MPARAM );

BOOL EXPENTRY RegisterInputBoxControl( HAB hab )
{
    return WinRegisterClass( hab, WC_INPUTBOX, ib_wndProc, CS_SIZEREDRAW, 4 );
}

HWND EXPENTRY IBCreateInputBox( HWND hwndOwner, ULONG id )
{
    return ( WinCreateWindow( HWND_DESKTOP, WC_INPUTBOX, "", 0, 0, 0,
                                    IB_CHAR_WIDTH + IB_BORDER_SIZE * 4,
                                    IB_CHAR_HEIGHT + IB_BORDER_SIZE * 4,
                                    hwndOwner, HWND_TOP, id, NULL, NULL ));

}

static MRESULT EXPENTRY ib_wndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg )
    {
        case WM_CREATE      : return ib_wmCreate( hwnd, mp1, mp2 );
        case WM_DESTROY     : return ib_wmDestroy( hwnd, mp1, mp2 );
        case WM_PAINT       : return ib_wmPaint( hwnd, mp1, mp2 );

        case IBM_SETHANCHAR     : return ib_umSetHanChar( hwnd, mp1, mp2 );
        case IBM_QUERYHANCHAR   : return ib_umQueryHanChar( hwnd, mp1, mp2 );
        case IBM_SHOWINPUTBOX   : return ib_umShowInputBox( hwnd, mp1, mp2 );
    }

    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

static MRESULT ib_wmCreate( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PINPUTBOXCTLDATA pibcd;

    WinSetWindowPtr( hwnd, 0, 0 );

    pibcd = malloc( sizeof( INPUTBOXCTLDATA ));
    if( pibcd == NULL )
        return MRFROMLONG( TRUE );

    if( DosAllocSharedMem(( PPVOID)&pibcd->pCursorPos, NULL, sizeof( RECTL ), fALLOCSHR ) != 0 )
    {
        free( pibcd );

        return MRFROMLONG( TRUE );
    }

    WinSetWindowPtr( hwnd, 0, pibcd );

    pibcd->hch = 0;

    return MRFROMLONG( FALSE );
}

static MRESULT ib_wmDestroy( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PINPUTBOXCTLDATA pibcd = WinQueryWindowPtr( hwnd, 0 );

    if( pibcd != NULL )
    {
        if( pibcd->pCursorPos != NULL )
            DosFreeMem( pibcd->pCursorPos );

        free( pibcd );
    }

    return 0;
}

static MRESULT ib_wmPaint( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PINPUTBOXCTLDATA pibcd = WinQueryWindowPtr( hwnd, 0 );
    HPS     hps;
    RECTL   rcl;
    POINTL  ptl;

    if( pibcd == NULL )
        return 0;

    hps = WinBeginPaint( hwnd, NULLHANDLE, &rcl);
    WinQueryWindowRect( hwnd, &rcl );

    WinFillRect( hps, &rcl, CLR_BLACK );

    GpiSetColor( hps, SYSCLR_BUTTONDARK );
    ptl.x = rcl.xRight - 1;
    ptl.y = rcl.yTop - 1;
    GpiMove( hps, &ptl );

    ptl.x = rcl.xLeft;
    GpiLine( hps, &ptl );

    ptl.y = rcl.yBottom;
    GpiLine( hps, &ptl );

    GpiSetColor( hps, SYSCLR_BUTTONMIDDLE );

    ptl.x = rcl.xLeft + 1;
    GpiMove( hps, &ptl );

    ptl.x = rcl.xRight - 1;
    GpiLine( hps, &ptl );

    ptl.y = rcl.yTop - 1;
    GpiLine( hps, &ptl );

    rcl.xLeft += IB_BORDER_SIZE;
    rcl.xRight -= IB_BORDER_SIZE;
    rcl.yBottom += IB_BORDER_SIZE;
    rcl.yTop -= IB_BORDER_SIZE;

    GpiSetColor( hps, SYSCLR_BUTTONMIDDLE );
    ptl.x = rcl.xRight - 1;
    ptl.y = rcl.yTop - 1;
    GpiMove( hps, &ptl );

    ptl.x = rcl.xLeft;
    GpiLine( hps, &ptl );

    ptl.y = rcl.yBottom;
    GpiLine( hps, &ptl );

    GpiSetColor( hps, SYSCLR_BUTTONDARK );

    ptl.x = rcl.xLeft + 1;
    GpiMove( hps, &ptl );

    ptl.x = rcl.xRight - 1;
    GpiLine( hps, &ptl );

    ptl.y = rcl.yTop - 1;
    GpiLine( hps, &ptl );

    if( pibcd->hch != 0 )
    {
        GpiSetColor( hps, CLR_WHITE );
        HanOutHch( hps, IB_BORDER_SIZE * 2, IB_BORDER_SIZE * 2, pibcd->hch );
    }

    WinEndPaint( hps );

    return 0;
}

static MRESULT ib_umSetHanChar( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PINPUTBOXCTLDATA pibcd = WinQueryWindowPtr( hwnd, 0 );

    if( pibcd == NULL )
        return 0;

    pibcd->hch = SHORT1FROMMP( mp1 );

    WinInvalidateRect( hwnd, NULL, FALSE );

    return 0;
}

static MRESULT ib_umQueryHanChar( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PINPUTBOXCTLDATA pibcd = WinQueryWindowPtr( hwnd, 0 );

    if( pibcd == NULL )
        return 0;

    return MRFROMSHORT( pibcd->hch );
}

static MRESULT ib_umShowInputBox( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PINPUTBOXCTLDATA pibcd = WinQueryWindowPtr( hwnd, 0 );
    HWND    hwndCurrentInput = HWNDFROMMP( mp1 );
    BOOL    flShow = LONGFROMMP( mp2 );
    USHORT  rc;

    if( flShow )
    {
        PID pid;

        WinQueryWindowProcess( hwndCurrentInput, &pid, NULL );
        DosGiveSharedMem( pibcd->pCursorPos, pid, PAG_READ | PAG_WRITE );

        rc = SHORT1FROMMR( WinSendMsg( hwndCurrentInput, WM_QUERYCONVERTPOS,
                                       MPFROMP( pibcd->pCursorPos ), 0 ));
        if( rc == QCP_CONVERT )
        {
            POINTL ptl = { pibcd->pCursorPos->xLeft, pibcd->pCursorPos->yBottom };

            if(( ptl.x != -1 ) && ( ptl.y != -1 ))
            {
                WinMapWindowPoints( hwndCurrentInput, HWND_DESKTOP, &ptl, 1 );

                ptl.x -= IB_BORDER_SIZE * 2;
                ptl.y -= IB_BORDER_SIZE * 2;
            }

            WinSetWindowPos( hwnd, HWND_TOP, ptl.x, ptl.y, 0, 0, SWP_MOVE | SWP_ZORDER );
        }
    }

    WinShowWindow( hwnd, flShow );

    return 0;
}


