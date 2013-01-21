#define DOS_MEMMGR
#define INCL_PM
#include <os2.h>

#include <string.h>

#include "kimedlg.h"
#include "../hanlib/han.h"

static POPTDLGPARAM param = NULL;

static MRESULT optDlg_wmInitDlg( HWND, MPARAM, MPARAM );
//static MRESULT optDlg_wmControl( HWND, MPARAM, MPARAM );
static MRESULT optDlg_wmCommand( HWND, MPARAM, MPARAM );
static MRESULT optDlg_wmDrawItem( HWND, MPARAM, MPARAM );
static MRESULT optDlg_wmMeasureItem( HWND, MPARAM, MPARAM );

MRESULT EXPENTRY optDlgProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg )
    {
        case WM_INITDLG     : return optDlg_wmInitDlg( hwnd, mp1, mp2 );
        //case WM_CONTROL     : return optDlg_wmControl( hwnd, mp1, mp2 );
        case WM_COMMAND     : return optDlg_wmCommand( hwnd, mp1, mp2 );
        case WM_DRAWITEM    : return optDlg_wmDrawItem( hwnd, mp1, mp2 );
        case WM_MEASUREITEM : return optDlg_wmMeasureItem( hwnd, mp1, mp2 );

    }

    return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

MRESULT optDlg_wmInitDlg( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    HPS             hps;
    LONG            count = 0;
    PFONTMETRICS    pfm;
    CHAR            fontName[ FACESIZE ];
    PCHAR           pch;
    LONG            cxScreen, cyScreen;
    RECTL           rcl;
    int             id;
    int             i;

    param = PVOIDFROMMP( mp2 );


    for( id = IDB_KBD2; id <= IDB_KBD3F; id++ )
        WinSendDlgItemMsg( hwnd, id, BM_SETCHECK,
                           MPFROMLONG( id == ( IDB_KBD2 + param->kbdLayout ) ? 1 : 0 ), 0 );

    WinSendDlgItemMsg( hwnd, IDB_3BUL, BM_SETCHECK, MPFROMLONG( param->patch3bul ), 0 );
    WinSendDlgItemMsg( hwnd, IDB_CHATLINE, BM_SETCHECK, MPFROMLONG( param->patchChat ), 0 );

    PrfQueryProfileString( HINI_USERPROFILE, "PM_SystemFonts", "PM_AssociateFont",
                           "¾øÀ½", fontName, FACESIZE );
    for( pch = fontName; ( *pch != 0 ) && (* pch != ';' ); pch ++ );
    if( *pch == ';' )
        *pch = 0;
    WinSetWindowText( WinWindowFromID( hwnd, IDHS_CURRENTFONT ), fontName );

    hps = WinGetPS( hwnd );
    count = GpiQueryFonts( hps, QF_PUBLIC | QF_PRIVATE, NULL, &count,
                           sizeof( FONTMETRICS ), NULL );

    DosAllocMem(( PPVOID ) &pfm, count * sizeof( FONTMETRICS ), fALLOC );

    GpiQueryFonts( hps, QF_PUBLIC | QF_NO_DEVICE, NULL, &count, sizeof( FONTMETRICS ), pfm );

    WinReleasePS( hps );

    for( i = 0; i < count; i++ )
    {
        if(( pfm[ i ].fsType & FM_TYPE_DBCS ) &&
           !strstr( pfm[ i ].szFacename, "Combined" ) &&
           ( strcmp( pfm[ i ].szFamilyname, "MINCHO" ) != 0 ))
            WinSendDlgItemMsg( hwnd, IDLB_FONTLIST, LM_INSERTITEM,
                               MPFROMSHORT( LIT_SORTASCENDING ), MPFROMP( pfm[ i ].szFacename ));
    }

    DosFreeMem(( VOID *) pfm );

    cxScreen = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    cyScreen = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

    WinQueryWindowRect( hwnd, &rcl );

    WinSetWindowPos( hwnd, HWND_TOP,
                     ( cxScreen - ( rcl.xRight - rcl.xLeft )) / 2,
                     ( cyScreen - ( rcl.yTop - rcl.yBottom )) / 2,
                     0, 0, SWP_MOVE );

    return 0;
}

/*
MRESULT optDlg_wmControl( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    if( SHORT1FROMMP( mp1 ) == IDLB_FONTLIST )
    {
        if( SHORT2FROMMP( mp1 ) == LN_ENTER )
        {
            SHORT index;

            index = SHORT1FROMMR( WinSendDlgItemMsg( hwnd, IDLB_FONTLIST, LM_QUERYSELECTION, MPFROMSHORT( LIT_FIRST ), 0 ));
            if( index != LIT_NONE )
            {
                CHAR fontName[ FACESIZE ];

                WinSendDlgItemMsg( hwnd, IDLB_FONTLIST, LM_QUERYITEMTEXT,
                                   MPFROM2SHORT( index, FACESIZE ), MPFROMP( fontName ));

                WinSetWindowText( WinWindowFromID( hwnd, IDHS_CURRENTFONT ), fontName );
            }
        }
    }

    return 0;
}
*/

MRESULT optDlg_wmCommand( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    if( SHORT1FROMMP( mp2 ) == CMDSRC_PUSHBUTTON )
    {
        if( SHORT1FROMMP( mp1 ) == IDB_FONTAPPLY)
        {
            SHORT index;

            index = SHORT1FROMMR( WinSendDlgItemMsg( hwnd, IDLB_FONTLIST, LM_QUERYSELECTION, MPFROMSHORT( LIT_FIRST ), 0 ));
            if( index != LIT_NONE )
            {
                CHAR fontName[ FACESIZE ];

                WinSendDlgItemMsg( hwnd, IDLB_FONTLIST, LM_QUERYITEMTEXT,
                                   MPFROM2SHORT( index, FACESIZE ), MPFROMP( fontName ));

                PrfWriteProfileString( HINI_USERPROFILE, "PM_SystemFonts", "PM_AssociateFont", fontName );
                WinMessageBox( HWND_DESKTOP, HWND_DESKTOP,
                               "System must be rebooted for changes to take effect.",
                               "Information", 100, MB_OK | MB_INFORMATION );

                WinSetWindowText( WinWindowFromID( hwnd, IDHS_CURRENTFONT ), fontName );
            }

            return 0;
        }
    }

    if( SHORT1FROMMP( mp1 ) == DID_OK )
    {
        param->kbdLayout = LONGFROMMR( WinSendDlgItemMsg( hwnd, IDB_KBD2, BM_QUERYCHECKINDEX, 0, 0 ));
        param->patch3bul = LONGFROMMR( WinSendDlgItemMsg( hwnd, IDB_3BUL, BM_QUERYCHECK, 0, 0 ));
        param->patchChat = LONGFROMMR( WinSendDlgItemMsg( hwnd, IDB_CHATLINE, BM_QUERYCHECK, 0, 0 ));

        WinDismissDlg( hwnd, DID_OK );
    }
    else
        return WinDefDlgProc( hwnd, WM_COMMAND, mp1, mp2 );

    return 0;
}

MRESULT optDlg_wmDrawItem( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    //USHORT      id = SHORT1FROMMP( mp1 );
    POWNERITEM  poi = ( POWNERITEM )mp2;
    LONG        fgColor, bgColor;
    int         maxLen = SHORT1FROMMR( WinSendMsg( poi->hwnd, LM_QUERYITEMTEXTLENGTH,
                                        MPFROMSHORT( poi->idItem ), 0 ));
    char        *str;

    DosAllocMem(( PPVOID )&str, maxLen + 1, fALLOC );
    if( str == NULL )
        return MRFROMLONG( FALSE );

    WinQueryLboxItemText( poi->hwnd, poi->idItem, str, maxLen + 1 );
    hch_ks2systr( str );

    fgColor = GpiQueryColor( poi->hps );
    bgColor = GpiQueryBackColor( poi->hps );

    if( poi->fsState )
    {
        GpiSetColor( poi->hps, bgColor );
        GpiSetBackColor( poi->hps, fgColor );
    }
    else
    {
        GpiSetColor( poi->hps, fgColor );
        GpiSetBackColor( poi->hps, bgColor );
    }

    WinFillRect( poi->hps, &poi->rclItem, poi->fsState ? fgColor : bgColor );
    HanOut( poi->hps, poi->rclItem.xLeft, poi->rclItem.yBottom, str );
    poi->fsState = poi->fsStateOld = FALSE;

    DosFreeMem( str );

    return MRFROMLONG( TRUE );
}


MRESULT optDlg_wmMeasureItem( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    return MRFROM2SHORT( 16, 0 );
}

