#define INCL_PM
#define INCL_DOS
#include <os2.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "khserver.h"
#include "hwndlist.h"
#include "except.h"
#include "dosqss.h"

#define EXCEPT_LIST_FILE    "KIMEXCPT.DAT"

#define DOSQSS_BUFSIZE  10240

typedef struct tagKHSCD
{
#ifdef DEBUG
    FILE        *fp;
#endif
    PRECTL      pCursorPos;
    PHWNDLIST   list;
    char        *exceptListBuf;
    PQTOPLEVEL  pQTopLevel;
} KHSCD, *PKHSCD;

static MRESULT khs_wmCreate( HWND, MPARAM, MPARAM );
static MRESULT khs_wmDestroy( HWND, MPARAM, MPARAM );

static MRESULT khs_umFindWnd( HWND, MPARAM, MPARAM );
static MRESULT khs_umAddWnd( HWND, MPARAM, MPARAM );
static MRESULT khs_umDelWnd( HWND, MPARAM, MPARAM );
static MRESULT khs_umQueryHanStatus( HWND, MPARAM, MPARAM );
static MRESULT khs_umChangeHanStatus( HWND, MPARAM, MPARAM );
static MRESULT khs_umSetHanStatus( HWND, MPARAM, MPARAM );
static MRESULT khs_umCheckDBCSSupport( HWND, MPARAM, MPARAM );
static MRESULT khs_umIsExceptWindow( HWND, MPARAM, MPARAM );
static MRESULT khs_umReloadExceptFile( HWND, MPARAM, MPARAM );

#ifdef DEBUG
static MRESULT khs_umStoreKeyInfo( HWND, MPARAM, MPARAM );
static MRESULT khs_umStoreMsg( HWND, MPARAM, MPARAM );
#endif

static MRESULT EXPENTRY khs_wndProc( HWND, ULONG, MPARAM, MPARAM );

BOOL EXPENTRY RegisterKimeHookServer( HAB hab )
{
    return WinRegisterClass( hab, WC_KIMEHOOKSERVER, khs_wndProc, CS_SIZEREDRAW, 4 );
}

HWND EXPENTRY KHSCreateKimeHookServer( HWND hwndOwner, ULONG id )
{
    return ( WinCreateWindow( HWND_OBJECT, WC_KIMEHOOKSERVER, "KimeHookServer",
                              0, 0, 0, 0, 0, hwndOwner, HWND_TOP, id, NULL, NULL ));

}

MRESULT EXPENTRY khs_wndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg )
    {
        case WM_CREATE  : return khs_wmCreate( hwnd, mp1, mp2 );
        case WM_DESTROY : return khs_wmDestroy( hwnd, mp1, mp2 );

        case KHSM_FINDWND           : return khs_umFindWnd( hwnd, mp1, mp2 );
        case KHSM_ADDWND            : return khs_umAddWnd( hwnd, mp1, mp2 );
        case KHSM_DELWND            : return khs_umDelWnd( hwnd, mp1, mp2 );
        case KHSM_QUERYHANSTATUS    : return khs_umQueryHanStatus( hwnd, mp1, mp2 );
        case KHSM_CHANGEHANSTATUS   : return khs_umChangeHanStatus( hwnd, mp1, mp2 );
        case KHSM_SETHANSTATUS      : return khs_umSetHanStatus( hwnd, mp1, mp2 );
        case KHSM_CHECKDBCSSUPPORT  : return khs_umCheckDBCSSupport( hwnd, mp1, mp2 );
        case KHSM_ISEXCEPTWINDOW    : return khs_umIsExceptWindow( hwnd, mp1, mp2 );
        case KHSM_RELOADEXCEPTFILE  : return khs_umReloadExceptFile( hwnd, mp1, mp2 );

#ifdef DEBUG
        case KHSM_STOREKEYINFO      : return khs_umStoreKeyInfo( hwnd, mp1, mp2 );
        case KHSM_STOREMSG          : return khs_umStoreMsg( hwnd, mp1, mp2 );
#endif
    }

    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


MRESULT khs_wmCreate( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd;

    WinSetWindowPtr( hwnd, 0, 0 );

    if( DosAllocMem(( PPVOID )&pkhscd, sizeof( KHSCD ), fALLOC ) != 0 )
        return MRFROMLONG( TRUE );

    if( DosAllocSharedMem(( PPVOID )&pkhscd->pCursorPos, NULL, sizeof( RECTL ), fALLOCSHR ) != 0 )
    {
        DosFreeMem( pkhscd );

        return MRFROMLONG( TRUE );
    }

    if( DosAllocMem(( PPVOID )&pkhscd->pQTopLevel, DOSQSS_BUFSIZE, fALLOC ) != 0 )
    {
        DosFreeMem( pkhscd->pCursorPos );
        DosFreeMem( pkhscd );

        return MRFROMLONG( TRUE );
    }

    pkhscd->list = hwndlistCreate();
    if( pkhscd->list == NULL )
    {
        DosFreeMem( pkhscd->pQTopLevel );
        DosFreeMem( pkhscd->pCursorPos );
        DosFreeMem( pkhscd );

        return MRFROMLONG( TRUE );
    }

#ifdef DEBUG
    pkhscd->fp = fopen("debug.log", "wt");
#endif
    pkhscd->exceptListBuf = exceptCreateListBuf( EXCEPT_LIST_FILE );

    WinSetWindowPtr( hwnd, 0, pkhscd );

    return FALSE;
}

MRESULT khs_wmDestroy( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd = WinQueryWindowPtr( hwnd, 0 );

#ifdef DEBUG
    fclose( pkhscd->fp );
#endif

    exceptDestroyListBuf( pkhscd->exceptListBuf );
    hwndlistDestroy( pkhscd->list );

    DosFreeMem( pkhscd->pQTopLevel );
    DosFreeMem( pkhscd->pCursorPos );
    DosFreeMem( pkhscd );

    return 0;
}

MRESULT khs_umFindWnd( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD      pkhscd = WinQueryWindowPtr( hwnd, 0 );
    HWND        hwndInput = HWNDFROMMP( mp1 );

    return MRFROMLONG( hwndlistSearch( pkhscd->list, hwndInput ) != NULL );
}

MRESULT khs_umAddWnd( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd = WinQueryWindowPtr( hwnd, 0 );
    HWND    hwndInput = HWNDFROMMP( mp1 );

    if(!( BOOL ) WinSendMsg( hwnd, KHSM_FINDWND, MPFROMHWND( hwndInput ), 0 ))
    {
        PHWNDLIST list = hwndlistCreate();

        if( list != NULL )
        {
            list->hwnd = hwndInput;
            hwndlistInsert( pkhscd->list, list );
        }
    }

    return 0;
}

MRESULT khs_umDelWnd( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd = WinQueryWindowPtr( hwnd, 0 );
    HWND    hwndInput = HWNDFROMMP( mp1 );

    if(( BOOL ) WinSendMsg( hwnd, KHSM_FINDWND, MPFROMHWND( hwndInput ), 0 ))
    {
        PHWNDLIST list;

        list = hwndlistSearch( pkhscd->list, hwndInput );
        hwndlistDelete( list );
    }

    return 0;
}

MRESULT khs_umQueryHanStatus( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd = WinQueryWindowPtr( hwnd, 0 );
    HWND    hwndInput = HWNDFROMMP( mp1 );

    if(( BOOL )WinSendMsg( hwnd, KHSM_FINDWND, MPFROMHWND( hwndInput ), 0 ))
    {
        PHWNDLIST list;

        list = hwndlistSearch( pkhscd->list, hwndInput );

        return MRFROMLONG( list->han );
    }

    return 0;
}

MRESULT khs_umChangeHanStatus( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd = WinQueryWindowPtr( hwnd, 0 );
    HWND    hwndInput = HWNDFROMMP( mp1 );

    if(( BOOL )WinSendMsg( hwnd, KHSM_FINDWND, MPFROMHWND( hwndInput ), 0 ))
    {
        PHWNDLIST list;

        list = hwndlistSearch( pkhscd->list, hwndInput );
        list->han = !list->han;
    }

    return 0;
}

MRESULT khs_umSetHanStatus( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd = WinQueryWindowPtr( hwnd, 0 );
    HWND    hwndInput = HWNDFROMMP( mp1 );
    BOOL    han = LONGFROMMP( mp2 );

    if(( BOOL )WinSendMsg( hwnd, KHSM_FINDWND, MPFROMHWND( hwndInput ), 0 ))
    {
        PHWNDLIST list;

        list = hwndlistSearch( pkhscd->list, hwndInput );
        list->han = han;
    }

    return 0;
}

MRESULT khs_umCheckDBCSSupport( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD      pkhscd = WinQueryWindowPtr( hwnd, 0 );
    HWND        hwndTarget = HWNDFROMMP( mp1 );
    PID         pid;
    USHORT      qcp;


    WinQueryWindowProcess( hwndTarget, &pid, NULL );
    DosGiveSharedMem( pkhscd->pCursorPos, pid, PAG_READ | PAG_WRITE );

    qcp = SHORT1FROMMR( WinSendMsg( hwndTarget, WM_QUERYCONVERTPOS,
                                   MPFROMP( pkhscd->pCursorPos ), 0 ));

    return MRFROMLONG( qcp == QCP_CONVERT );
}

MRESULT khs_umIsExceptWindow( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD      pkhscd = WinQueryWindowPtr( hwnd, 0 );
    HWND        hwndTarget = HWNDFROMMP( mp1 );
    PID         pid;
    PCHAR       modulePath;
    PCHAR       moduleName;
    ULONG       maxPathLen;
    APIRET      rc;
    BOOL        result;

    WinQueryWindowProcess( hwndTarget, &pid, NULL );

#ifdef DEBUG
    fprintf( pkhscd->fp, "PID = %x\n", pid );
#endif

    memset( pkhscd->pQTopLevel, 0, DOSQSS_BUFSIZE );

    rc = DosQuerySysState( 0x01, 0, pid, 1, pkhscd->pQTopLevel, DOSQSS_BUFSIZE );
    if( rc != 0 )
        return FALSE;

    DosQuerySysInfo( QSV_MAX_PATH_LENGTH, QSV_MAX_PATH_LENGTH, &maxPathLen, sizeof( ULONG ));

    modulePath = malloc( maxPathLen );
    DosQueryModuleName( pkhscd->pQTopLevel->procdata->hndmod, maxPathLen, modulePath );

#ifdef DEBUG
    fprintf( pkhscd->fp, "Module path = %s\n", modulePath );
#endif

    moduleName = strrchr( modulePath, '\\' );
    if( moduleName == NULL )
        moduleName = modulePath;
    else
        moduleName++;

#ifdef DEBUG
    fprintf( pkhscd->fp, "Module name = %s\n", moduleName );
#endif

    result = exceptFindName( pkhscd->exceptListBuf, moduleName );

    free( modulePath );

#ifdef DEBUG
    fprintf( pkhscd->fp, "exceptFindName() = %ld\n", result );

    fflush( pkhscd->fp );
#endif

    return MRFROMLONG( result );
}

MRESULT khs_umReloadExceptFile( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD      pkhscd = WinQueryWindowPtr( hwnd, 0 );

    exceptDestroyListBuf( pkhscd->exceptListBuf );

    pkhscd->exceptListBuf = exceptCreateListBuf( EXCEPT_LIST_FILE );
}

#ifdef DEBUG
MRESULT khs_umStoreKeyInfo( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd = WinQueryWindowPtr( hwnd, 0 );
    USHORT  fsFlags = SHORT1FROMMP( mp1 );
    UCHAR   ucRepeat = CHAR3FROMMP( mp1 );
    UCHAR   ucScancode = CHAR4FROMMP( mp1 );
    USHORT  usCh = SHORT1FROMMP( mp2 );
    USHORT  usVk = SHORT2FROMMP( mp2 );

    fprintf( pkhscd->fp, "fl : %04X, rp : %02d, sc : %02X, ch : %02X, vk : %02X\n",
                         fsFlags, ucRepeat, ucScancode, usCh, usVk );
    fflush( pkhscd->fp );

    return 0;
}

MRESULT khs_umStoreMsg( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKHSCD  pkhscd = WinQueryWindowPtr( hwnd, 0 );

    DosGetSharedMem( mp1, PAG_READ | PAG_WRITE );

    fprintf( pkhscd->fp, "%s", ( PSZ )mp1 );
    fflush( pkhscd->fp );

    return 0;
}

#endif
