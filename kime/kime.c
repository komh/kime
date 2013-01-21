// 2001.05.25
#define INCL_DOSPROCESS
#define INCL_WIN
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kime.h"
#include "kimedlg.h"
#include "kimehook.h"

#include "inputbox.h"
#include "../hanlib/han.h"
#include "../hst/hst.h"

#define KIME_NAME   "K Input Method Editor"

#define PRF_APP             "KIME"
#define PRF_KEY_POSITION    "Position"
#define PRF_KEY_KBDLAYOUT   "KbdLayout"
#define PRF_KEY_PATCH3BUL   "Patch3Bul"
#define PRF_KEY_PATCHCHAT   "PatchChat"
#define PRF_KEY_USEOS2IME   "UseOS2IME"

//#define ADD_TO_SWITCH_ENTRY

#define BUTTON_COUNT    2               // han/eng, char/line
#define BORDER_SIZE     1

#define EXTRA_WIDTH     1
#define EXTRA_HEIGHT    1
#define WIN_WIDTH       (( BORDER_SIZE * 2 ) + ( charWidth * 3 ) + ( EXTRA_WIDTH * 2 ) +\
                         ( charWidth * 4 ) + ( EXTRA_WIDTH * 2 ))
#define WIN_HEIGHT      (( BORDER_SIZE * 2 ) + charHeight * 2 + ( EXTRA_HEIGHT * 2 ))

typedef struct tagKIME {
    BOOL    han;
    BOOL    line;
    HWND    hwndHIA;
    HWND    hwndIB;
    HWND    hwndKHS;
} KIME, *PKIME;

static const char *hanStatusStr[] = { "Eng", "Han", };
static const char *imStatusStr[] = { "Char", "Line", };

static HWND hwndPopup = NULLHANDLE;

static PFNWP oldButtonWndProc = NULL;

static PVOID checkKime = NULL;

static PSZ  hanjafontpath = NULL;

static VOID run( HAB );
static VOID destroy( VOID );
static BOOL alreadyLoaded( VOID );
static VOID loadOpt( POPTDLGPARAM );
static VOID saveOpt( POPTDLGPARAM );
static VOID processArg( INT, CHAR ** );
static VOID queryCharSize( HWND, LONG *, LONG * );

static MRESULT EXPENTRY newButtonWndProc( HWND, ULONG, MPARAM, MPARAM );

static MRESULT EXPENTRY windowProc( HWND, ULONG, MPARAM, MPARAM );

static MRESULT wmCreate( HWND, MPARAM, MPARAM );
static MRESULT wmDestroy( HWND, MPARAM, MPARAM );
static MRESULT wmSize( HWND, MPARAM, MPARAM );
static MRESULT wmPaint( HWND, MPARAM, MPARAM );
static MRESULT wmBeginDrag( HWND, MPARAM, MPARAM );
static MRESULT wmButton2Up( HWND, MPARAM, MPARAM );
static MRESULT wmCommand( HWND, MPARAM, MPARAM );

static MRESULT kime_umChangeHan( HWND, MPARAM, MPARAM );
static MRESULT kime_umSetHan( HWND, MPARAM, MPARAM );
static MRESULT kime_umQueryHan( HWND, MPARAM, MPARAM );
static MRESULT kime_umChangeIm( HWND, MPARAM, MPARAM );
static MRESULT kime_umSetIm( HWND, MPARAM, MPARAM );
static MRESULT kime_umQueryIm( HWND, MPARAM, MPARAM );

INT main( int argc, char **argv )
{
    _envargs( &argc, &argv, "KIMEOPT");
    processArg( argc, argv );

    if( !alreadyLoaded())
    {
        HAB     hab;
        HMQ     hmq;

        hab = WinInitialize( 0 );
        hmq = WinCreateMsgQueue( hab, 0);

        run( hab );
        destroy();

        WinDestroyMsgQueue( hmq );
        WinTerminate( hab );

    }

    return 0;
}

VOID run( HAB hab )
{
    HWND    hwnd;
    QMSG    qm;

    WinRegisterClass(
        hab,
        WC_KIME,
        windowProc,
        CS_SIZEREDRAW,
        sizeof( PVOID )
    );

    if( !RegisterHanStaticTextControl( hab ))
        return;

    if( !RegisterHanAutomataClass( hab ))
        return;

    if( !RegisterInputBoxControl( hab ))
        return;

    if( !RegisterKimeHookServer( hab ))
        return;

    hwnd = WinCreateWindow( HWND_DESKTOP, WC_KIME, KIME_NAME, 0,
                            0, 0, 0, 0, NULLHANDLE, HWND_TOP, ID_KIME,
                            NULL, NULL );

    if( hwnd != NULLHANDLE )
    {
        LONG charWidth, charHeight;

        queryCharSize( hwnd, &charWidth, &charHeight );

        if( !WinRestoreWindowPos( PRF_APP, PRF_KEY_POSITION, hwnd ))
           WinSetWindowPos( hwnd, HWND_TOP, 0, 0, WIN_WIDTH, WIN_HEIGHT, SWP_MOVE | SWP_SIZE );

        WinInvalidateRect( hwnd, NULL, TRUE );
        WinSetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOW | SWP_ZORDER );

        houtInit( hab, 128 );
        if( hanjafontpath != NULL )
        {
            static char external_hanjafontbuf[HOUT_HANJAFONTBUFSIZE];

            houtReadHanjaFont( hanjafontpath,external_hanjafontbuf);
            houtSetHanjaFont(external_hanjafontbuf);
        }

        while( WinGetMsg( hab, &qm, NULLHANDLE, 0, 0 ))
            WinDispatchMsg( hab, &qm );

        houtClose();

        WinStoreWindowPos( PRF_APP, PRF_KEY_POSITION, hwnd );

        WinDestroyWindow( hwnd );
    }
}

VOID destroy( VOID )
{
    DosFreeMem( checkKime );
}

BOOL alreadyLoaded( VOID )
{
    PSZ     pszKimeSig = "\\SHAREMEM\\KIME.SIG";
    APIRET  rc;

    rc = DosGetNamedSharedMem( &checkKime, pszKimeSig, PAG_READ );
    if( rc == 0 )
        return TRUE;

    DosAllocSharedMem( &checkKime, pszKimeSig, sizeof( HAB ), fALLOC );

    return FALSE;
}

VOID loadOpt( POPTDLGPARAM pOpt )
{
    kimeOpt.kbdLayout = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_KBDLAYOUT, KL_KBD2 );
    kimeOpt.patch3bul = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_PATCH3BUL, TRUE );
    kimeOpt.patchChat = PrfQueryProfileInt( HINI_USERPROFILE, PRF_APP, PRF_KEY_PATCHCHAT, TRUE );
}

VOID saveOpt( POPTDLGPARAM pOpt )
{
    CHAR    szOptStr[ 20 ];

    _ltoa( kimeOpt.kbdLayout, szOptStr, 10 );
    PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_KBDLAYOUT, szOptStr );

    _ltoa( kimeOpt.patch3bul, szOptStr, 10 );
    PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_PATCH3BUL, szOptStr );

    _ltoa( kimeOpt.patchChat, szOptStr, 10 );
    PrfWriteProfileString( HINI_USERPROFILE, PRF_APP, PRF_KEY_PATCHCHAT, szOptStr );
}

VOID processArg( INT argc, CHAR **argv )
{
    INT i;

    loadOpt( &kimeOpt );

    for( i = 1; i < argc; i ++ )
    {
        if( strcmp( argv[ i ], "--no-3bul" ) == 0 )
            kimeOpt.patch3bul = FALSE;
        else if( strcmp( argv[ i ], "--no-chatline" ) == 0 )
            kimeOpt.patchChat = FALSE;
        else if( strcmp( argv[ i ], "--kbd390" ) == 0 )
            kimeOpt.kbdLayout = KL_KBD390;
        else if( strcmp( argv[ i ], "--kbd3f" ) == 0 )
            kimeOpt.kbdLayout = KL_KBD3F;
        else if( strcmp( argv[ i ], "--hanjafont" ) == 0 )
        {
            i++;
            hanjafontpath = argv[ i ];
        }

    }
}

VOID queryCharSize( HWND hwnd, LONG *charWidth, LONG *charHeight )
{
    HPS         hps;
    FONTMETRICS fm;

    hps = WinGetPS( hwnd );

    GpiQueryFontMetrics( hps, sizeof( FONTMETRICS ), &fm );

    WinReleasePS( hps );

    *charWidth = fm.lAveCharWidth + fm.lMaxCharInc / 2;
    *charHeight = fm.lMaxAscender + fm.lMaxDescender;
}

MRESULT EXPENTRY newButtonWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    if( msg == WM_BEGINDRAG )
    {
        HWND hwndOwner = WinQueryWindow( hwnd, QW_OWNER );

        if( hwndOwner != NULLHANDLE )
        {
            WinPostMsg( hwndOwner, msg, mp1, mp2 );

            return MRFROMLONG( TRUE );
        }
    }

    return oldButtonWndProc( hwnd, msg, mp1,mp2 );
}

MRESULT EXPENTRY windowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    switch( msg )
    {
        case WM_CREATE          : return wmCreate( hwnd, mp1, mp2 );
        case WM_DESTROY         : return wmDestroy( hwnd, mp1, mp2 );
        case WM_SIZE            : return wmSize( hwnd, mp1, mp2 );
        case WM_PAINT           : return wmPaint( hwnd, mp1, mp2 );
        case WM_BEGINDRAG       : return wmBeginDrag( hwnd, mp1, mp2 );
        case WM_BUTTON2UP       : return wmButton2Up( hwnd, mp1, mp2 );
        case WM_COMMAND         : return wmCommand( hwnd, mp1, mp2 );
        case KIMEM_CHANGEHAN     : return kime_umChangeHan( hwnd, mp1, mp2 );
        case KIMEM_SETHAN        : return kime_umSetHan( hwnd, mp1, mp2 );
        case KIMEM_QUERYHAN      : return kime_umQueryHan( hwnd, mp1, mp2 );
        case KIMEM_CHANGEIM      : return kime_umChangeIm( hwnd, mp1, mp2 );
        case KIMEM_SETIM         : return kime_umSetIm( hwnd, mp1, mp2 );
        case KIMEM_QUERYIM       : return kime_umQueryIm( hwnd, mp1, mp2 );
    }

    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

MRESULT wmCreate( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKIME kime;
    PCREATESTRUCT pcs = ( PCREATESTRUCT )mp2;
#ifdef ADD_TO_SWITCH_ENTRY
    SWCNTRL swc;
#endif
    LONG x = BORDER_SIZE * 2;
    LONG y = BORDER_SIZE * 2;
    LONG cx = ( pcs->cx - BORDER_SIZE * 3 * 2 ) / BUTTON_COUNT;
    LONG cy = ( pcs->cy - BORDER_SIZE * 2 * 2 );
    HWND hwndBtn;
    HOOKDATA hd;

    WinSetWindowPtr( hwnd, 0, NULL );

    kime = malloc( sizeof( KIME ));
    if( kime == NULL )
        return MRFROMLONG( TRUE );

    hd.hwndKime = hwnd;
    hd.hwndHIA = kime->hwndHIA = HIACreateHanAutomata( hwnd, ID_HIA );
    hd.hwndIB = kime->hwndIB = IBCreateInputBox( hwnd, ID_INPUTBOX );
    hd.hwndKHS = kime->hwndKHS = KHSCreateKimeHookServer( hwnd, ID_KHS );

    WinSendMsg( kime->hwndHIA, HIAM_SETKBDTYPE, MPFROMLONG( kimeOpt.kbdLayout ), 0 );

    if( !installHook( WinQueryAnchorBlock( hwnd ), &hd ))
    {
        free( kime );

        return MRFROMLONG( TRUE );
    }

    WinSendMsg( kime->hwndHIA, HIAM_CONNECT, MPFROMLONG( hwnd ), MPFROMLONG( ID_HIA ));

    kime->han = FALSE;
    kime->line = FALSE;
    WinSetWindowPtr( hwnd, 0, kime );

#ifdef ADD_TO_SWITCH_ENTRY
    swc.hwnd = hwnd;
    swc.hwndIcon = NULLHANDLE;
    swc.hprog = NULLHANDLE;
    swc.idProcess = 0;
    swc.idSession = 0;
    swc.uchVisibility = SWL_VISIBLE;
    swc.fbJump = SWL_JUMPABLE;
    swc.bProgType = PROG_DEFAULT;
    strcpy( swc.szSwtitle, pcs->pszText );

    WinAddSwitchEntry( &swc );
#endif

    hwndBtn = WinCreateWindow( hwnd, WC_BUTTON, hanStatusStr[ kime->han ], WS_VISIBLE,
                     x, y, cx, cy, hwnd, HWND_TOP, IDB_HANENG,
                     NULL, NULL );

    oldButtonWndProc = WinSubclassWindow( hwndBtn, newButtonWndProc );

    x += cx + BORDER_SIZE * 2;
    hwndBtn = WinCreateWindow( hwnd, WC_BUTTON, imStatusStr[ kime->line ], WS_VISIBLE,
                     x, y, cx, cy, hwnd, HWND_TOP, IDB_IM,
                     NULL, NULL );

    oldButtonWndProc = WinSubclassWindow( hwndBtn, newButtonWndProc );


/*
    x += cx;
    WinCreateWindow( hwnd, WC_BUTTON, "KIME", WS_VISIBLE,
                     x, y, cx, cy, hwnd, HWND_TOP, IDB_KIME,
                     NULL, NULL );
*/

    hwndPopup = WinLoadMenu( hwnd, NULLHANDLE, ID_POPUP );

    return 0;
}

MRESULT wmDestroy( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKIME kime = WinQueryWindowPtr( hwnd, 0 );

    if( kime != NULL )
    {
        if( kime->hwndHIA != NULLHANDLE )
            WinSendMsg( kime->hwndHIA, HIAM_UNREGISTERNOTIFY, MPFROMLONG( hwnd ), 0);

#ifdef ADD_TO_SWITCH_ENTRY
        WinRemoveSwitchEntry( WinQuerySwitchHandle( hwnd, 0 ));
#endif
        free( kime );
    }

    uninstallHook();

    return MRFROMSHORT( FALSE );
}

MRESULT wmSize( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    LONG x = BORDER_SIZE * 2;
    LONG y = BORDER_SIZE * 2;
    LONG cx = ( SHORT1FROMMP( mp2 ) - BORDER_SIZE * 3 * 2 ) / BUTTON_COUNT;
    LONG cy = ( SHORT2FROMMP( mp2 ) - BORDER_SIZE * 2 * 2 );

    WinSetWindowPos( WinWindowFromID( hwnd, IDB_HANENG ), HWND_TOP,
                     x, y, cx, cy, SWP_MOVE | SWP_SIZE );

    x += cx + BORDER_SIZE * 2;
    WinSetWindowPos( WinWindowFromID( hwnd, IDB_IM ), HWND_TOP,
                     x, y, cx, cy, SWP_MOVE | SWP_SIZE );

/*
    x += cx;
    WinSetWindowPos( WinWindowFromID( hwnd, IDB_KIME ), HWND_TOP,
                     x, y, cx, cy, SWP_MOVE | SWP_SIZE );
*/

    return 0;
}

MRESULT wmPaint( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    HPS     hps;
    RECTL   rcl;
    POINTL  ptl;
    POINTL  ptlMid;

    hps = WinBeginPaint( hwnd, NULLHANDLE, &rcl);
    WinQueryWindowRect( hwnd, &rcl );

    ptlMid.x = BORDER_SIZE * 2 + (( rcl.xRight - rcl.xLeft ) - BORDER_SIZE * 3 * 2) / BUTTON_COUNT;
    ptlMid.y = BORDER_SIZE * 2;

//    WinFillRect( hps, &rcl, SYSCLR_BUTTONDARK );

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

    rcl.xLeft += BORDER_SIZE;
    rcl.xRight -= BORDER_SIZE;
    rcl.yBottom += BORDER_SIZE;
    rcl.yTop -= BORDER_SIZE;

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

    GpiSetColor( hps, SYSCLR_BUTTONDARK );

    GpiMove( hps, &ptlMid );
    ptl.x = ptlMid.x;
    ptl.y --;
    GpiLine( hps, &ptl );

    GpiSetColor( hps, SYSCLR_BUTTONMIDDLE );

    ptlMid.x ++;
    GpiMove( hps, &ptlMid );
    ptl.x = ptlMid.x;
    ptl.y++;
    GpiLine( hps, &ptl );

    WinEndPaint( hps );

    return MRFROMSHORT( 0 );
}

MRESULT wmBeginDrag( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    TRACKINFO   trackInfo;
    SWP         swp;

    trackInfo.cxBorder = 1;
    trackInfo.cyBorder = 1;
    trackInfo.cxGrid = 1;
    trackInfo.cyGrid = 1;
    trackInfo.cxKeyboard = 8;
    trackInfo.cyKeyboard = 8;

    WinQueryWindowPos( hwnd, &swp );
    trackInfo.rclTrack.xLeft = swp.x;
    trackInfo.rclTrack.xRight = swp.x + swp.cx;
    trackInfo.rclTrack.yBottom = swp.y;
    trackInfo.rclTrack.yTop = swp.y + swp.cy;

    WinQueryWindowPos( HWND_DESKTOP, &swp );
    trackInfo.rclBoundary.xLeft = swp.x;
    trackInfo.rclBoundary.xRight = swp.x + swp.cx;
    trackInfo.rclBoundary.yBottom = swp.y;
    trackInfo.rclBoundary.yTop = swp.y + swp.cy;

    trackInfo.ptlMinTrackSize.x = 0;
    trackInfo.ptlMinTrackSize.y = 0;

    trackInfo.ptlMaxTrackSize.x = swp.cx;
    trackInfo.ptlMaxTrackSize.y = swp.cy;

    trackInfo.fs = TF_MOVE | TF_STANDARD | TF_ALLINBOUNDARY;

    if ( WinTrackRect( HWND_DESKTOP, NULLHANDLE, &trackInfo ))
    {
        WinInvalidateRect( hwnd, NULL, TRUE );
        WinSetWindowPos( hwnd, HWND_TOP,
                         trackInfo.rclTrack.xLeft, trackInfo.rclTrack.yBottom, 0, 0,
                         SWP_SHOW | SWP_MOVE | SWP_ZORDER );
        WinStoreWindowPos( PRF_APP, PRF_KEY_POSITION, hwnd );
    }

    return MRFROMLONG( TRUE );
}

MRESULT wmButton2Up( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    ULONG fs = PU_NONE | PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_HCONSTRAIN | PU_VCONSTRAIN;

    if( WinIsWindowEnabled( hwnd ))
    {
        WinPopupMenu( hwnd, hwnd, hwndPopup,
                      SHORT1FROMMP( mp1 ), SHORT2FROMMP( mp1 ),
                      0, fs );
    }

    return MRFROMLONG( TRUE );
}

MRESULT wmCommand( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    switch( SHORT1FROMMP( mp2 ))
    {
        case CMDSRC_PUSHBUTTON :
            switch( SHORT1FROMMP( mp1 ))
            {
                case IDB_HANENG :
                    WinInvalidateRect( hwnd, NULL, TRUE );
                    WinSetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOW | SWP_ZORDER );
                    //WinSendMsg( hwnd, KIMEM_CHANGEHAN, 0, 0 );
                    break;

                case IDB_IM :
                    WinInvalidateRect( hwnd, NULL, TRUE );
                    WinSetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOW | SWP_ZORDER );
                    //WinSendMsg( hwnd, KIMEM_CHANGEIM, 0, 0 );
                    break;

/*
                case IDB_KIME :
                    break;
*/
            }
            break;

        case CMDSRC_MENU :
            switch( SHORT1FROMMP( mp1 ))
            {
                case IDM_HIDE :
                    if( WinQueryFocus( HWND_DESKTOP ) == hwnd )
                        WinSetFocus( HWND_DESKTOP, WinQueryWindow( hwnd, QW_OWNER ));

                    WinShowWindow( hwnd, FALSE );
                    break;

                case IDM_OPTIONS :
                {
                    PKIME kime = WinQueryWindowPtr( hwnd, 0 );

                    if( WinDlgBox( HWND_DESKTOP, hwnd, optDlgProc, NULLHANDLE, IDD_OPTDLG, &kimeOpt ) == DID_OK )
                    {
                        saveOpt( &kimeOpt );

                        WinSendMsg( kime->hwndHIA, HIAM_SETKBDTYPE, MPFROMLONG( kimeOpt.kbdLayout ), 0 );
                    }

                    break;
                }

                case IDM_EXIT :
                    WinPostMsg( hwnd, WM_QUIT, 0, 0 );
                    break;
            }
            break;
    }

    return 0;
}

MRESULT kime_umChangeHan( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKIME kime = WinQueryWindowPtr( hwnd, 0 );
    HWND hwndHanEngBtn = WinWindowFromID( hwnd, IDB_HANENG );

    WinInvalidateRect( hwnd, NULL, TRUE );
    WinSetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOW | SWP_ZORDER );

    kime->han = !kime->han;
    WinSetWindowText( hwndHanEngBtn, hanStatusStr[ kime->han ]);

    return 0;
}

MRESULT kime_umSetHan( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKIME kime = WinQueryWindowPtr( hwnd, 0 );
    HWND hwndHanEngBtn = WinWindowFromID( hwnd, IDB_HANENG );

    WinInvalidateRect( hwnd, NULL, TRUE );
    WinSetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOW | SWP_ZORDER );

    kime->han = LONGFROMMP( mp1 );
    WinSetWindowText( hwndHanEngBtn, hanStatusStr[ kime->han ]);

    return 0;
}

MRESULT kime_umQueryHan( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKIME kime = WinQueryWindowPtr( hwnd, 0 );

    return MRFROMLONG( kime->han );
}

MRESULT kime_umChangeIm( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKIME kime = WinQueryWindowPtr( hwnd, 0 );
    HWND hwndImBtn = WinWindowFromID( hwnd, IDB_IM );

    kime->line = !kime->line;
    WinSetWindowText( hwndImBtn, imStatusStr[ kime->line ]);

    return 0;
}

MRESULT kime_umSetIm( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKIME kime = WinQueryWindowPtr( hwnd, 0 );
    HWND hwndImBtn = WinWindowFromID( hwnd, IDB_IM );

    kime->line = LONGFROMMP( mp1 );
    WinSetWindowText( hwndImBtn, imStatusStr[ kime->line ]);

    return 0;
}

MRESULT kime_umQueryIm( HWND hwnd, MPARAM mp1, MPARAM mp2 )
{
    PKIME kime = WinQueryWindowPtr( hwnd, 0 );

    return MRFROMLONG( kime->line );
}


