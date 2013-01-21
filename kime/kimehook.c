/*
        DLL for K Input Method Editor
        Copyright (C) 2001 by KO Myung-Hun

        This library is free software; you can redistribute it and/or
        modify it under the terms of the GNU Library General Public
        License as published by the Free Software Foundation; either
        version 2 of the License, or any later version.

        This library is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
        Library General Public License for more details.

        You should have received a copy of the GNU Library General Public
        License along with this library; if not, write to the Free
        Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Change Log :

        Written by KO Myung-Hun
        Term of programming : 2001.10.20

        Source file   : kimehook.c
        Used compiler : emx 0.9d + gcc 2.8.1
*/
#define INCL_DOSMODULEMGR
#define INCL_PM
#include <os2.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "kime.h"
#include "kimehook.h"
#include "kimeconv.inc"

#include "inputbox.h"
#include "../hanlib/han.h"

#include "../hchlb/hchlbdlg.h"

#ifdef DEBUG
#include <stdarg.h>

#define dprintf( a ) storeMsg a
#else
#define dprintf( a )
#endif

#define FOCUS_ON

#ifndef HK_ACCEL
#define HK_ACCEL    17
#endif

#define DLL_NAME    "KIMEHOOK"

#define WC_ZTELNET  "ClientWindowClass"
#define WC_HCHLB    "HanCharListBox"

#define SBCS_CHARS  "`1234567890-=\\~!@#$%^&*()_+|[]{};':\",./<>?"

OPTDLGPARAM kimeOpt = { KL_KBD2, TRUE, TRUE };

static HAB hab = 0;
static HWND hwndKime = NULLHANDLE;
static HWND hwndHIA = NULLHANDLE;
static HWND hwndIB = NULLHANDLE;
static HWND hwndKHS = NULLHANDLE;
static HWND hwndCurrentInput = NULLHANDLE;

static HMODULE hm = NULLHANDLE;
static UCHAR uchPrevDbl = 0;
static BOOL dblJaumPressed = FALSE;
static BOOL prevHanInput = FALSE;

static HANCHAR hchComposing = 0;
static BOOL supportDBCS = FALSE;
static BOOL exception = FALSE;

#ifdef FOCUS_ON
static BOOL inputFocusChanged = FALSE;
#endif

static PFNWP oldKimeWndProc = NULL;
static VOID sendCharToWnd( HANCHAR hch );
static MRESULT EXPENTRY newKimeWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );

#define WM_CHAR_SPECIAL               0x04ba     // for Odin

#define ALARM   WinAlarm( HWND_DESKTOP, WA_NOTE )

#define setInputBoxHch( hch ) \
    ( WinPostMsg( hwndIB, IBM_SETHANCHAR, MPFROMSHORT( hch ), 0 ))
#define showInputBox( flShow ) \
    ( WinPostMsg( hwndIB, IBM_SHOWINPUTBOX, MPFROMHWND( hwndCurrentInput ), MPFROMLONG( flShow )))

#define findWnd( hwnd ) (( BOOL )WinSendMsg( hwndKHS, KHSM_FINDWND, MPFROMHWND( hwnd ), 0 ))
#define addWnd( hwnd ) ( WinSendMsg( hwndKHS, KHSM_ADDWND, MPFROMHWND( hwnd ), 0 ))
#define delWnd( hwnd ) ( WinSendMsg( hwndKHS, KHSM_DELWND, MPFROMHWND( hwnd ), 0 ))
#define checkDBCSSupport( hwnd ) ( LONGFROMMR( WinSendMsg( hwndKHS, KHSM_CHECKDBCSSUPPORT, MPFROMHWND( hwnd ), 0 )))
#define checkExceptWindow( hwnd ) ( LONGFROMMR( WinSendMsg( hwndKHS, KHSM_ISEXCEPTWINDOW, MPFROMHWND( hwnd ), 0 )))

#define queryRunningHCHLB() (( BOOL )WinSendMsg( hwndHIA, HIAM_QUERYRUNNINGHCHLB, 0, 0 ))

static BOOL isDblJaum( UCHAR uch );
static BOOL isZtelnet( HWND hwnd );
//static BOOL isKimeProcess( HWND hwnd );
static UCHAR *findKbdConv( UCHAR uch );
static USHORT kbdKeyTranslate( PQMSG pQmsg );
static BOOL isCapsLockOn( VOID );
static BOOL ztelnetAccelHook( PQMSG pQmsg );
static BOOL kimeAccelHook( PQMSG pQmsg );
static VOID ztelnetSendMsgHook( PSMHSTRUCT psmh );
static VOID kimeSendMsgHook( PSMHSTRUCT psmh );
static VOID initKimeStatus( HWND hwnd, BOOL ztelnet );
static BOOL isHanjaKey( USHORT flags, UCHAR ucScancode, USHORT usVk, USHORT usCh );
static BOOL isSpecialCharKey( USHORT flags, UCHAR ucScancode, USHORT usVk, USHORT usCh );
static BOOL isHCHLB( HWND hwnd );

#ifdef DEBUG
static VOID storeMsg( char *format, ... );
#endif

BOOL EXPENTRY inputHook( HAB hab, PQMSG pQmsg, USHORT fsOptions )
{
    if(( pQmsg->msg >= WM_BUTTONCLICKFIRST ) && ( pQmsg->msg <= WM_BUTTONCLICKLAST ))
    {
        ULONG flHIAState = (ULONG) WinSendMsg( hwndHIA, HIAM_QUERYSTATE, 0L, 0L );
        BOOL fRunningHCHLB = queryRunningHCHLB();

        if( HIUSHORT( flHIAState ) || fRunningHCHLB )
        {
            if( !fRunningHCHLB )
            {
                WinSendMsg( hwndHIA, HIAM_COMPLETEHCH, 0, 0 );

                WinPostMsg( pQmsg->hwnd, pQmsg->msg, pQmsg->mp1, pQmsg->mp2 );
                return TRUE;
            }

            if( !isHCHLB( pQmsg->hwnd ))
            {
                WinSendMsg( hwndHIA, HIAM_DESTROYHCHLB, 0, 0 );

                WinPostMsg( pQmsg->hwnd, pQmsg->msg, pQmsg->mp1, pQmsg->mp2 );
                return TRUE;
            }
        }
    }
#if 0
    else if( pQmsg->msg == WM_CHAR )
    {
        USHORT  fsFlags = SHORT1FROMMP( pQmsg->mp1 );
        UCHAR   ucRepeat = CHAR3FROMMP( pQmsg->mp1 );
        UCHAR   ucScancode = CHAR4FROMMP( pQmsg->mp1 );
        USHORT  usCh = SHORT1FROMMP( pQmsg->mp2 );
        USHORT  usVk = SHORT2FROMMP( pQmsg->mp2 );

        if(( fsFlags & KC_SCANCODE ) && ( ucScancode == 0x2B ) && ( fsFlags & KC_INVALIDCHAR ))
        {
            fsFlags &= ~KC_INVALIDCHAR;
            fsFlags |= KC_CHAR;

            usCh = 0x5C;

            pQmsg->mp1 = MPFROMSH2CH( fsFlags, ucRepeat, ucScancode );
            pQmsg->mp2 = MPFROM2SHORT( usCh, usVk );

            WinPostMsg( pQmsg->hwnd, WM_CHAR, pQmsg->mp1, pQmsg->mp2 );

            return TRUE;
        }
    }
#endif
    return FALSE;
}

BOOL EXPENTRY accelHook( HAB hab, PQMSG pQmsg, USHORT fsOptions )
{
    if( isZtelnet( pQmsg->hwnd ))
        return ztelnetAccelHook( pQmsg );

    return kimeAccelHook( pQmsg );
}

VOID EXPENTRY sendMsgHook( HAB hab, PSMHSTRUCT psmh, BOOL fInterTask )
{
    if( isZtelnet( psmh->hwnd ))
        ztelnetSendMsgHook( psmh );
    else
        kimeSendMsgHook( psmh );
}

BOOL EXPENTRY destroyWindowHook( HAB hab, HWND hwnd, ULONG ulReserved )
{
    if( findWnd( hwnd ))
        delWnd( hwnd );

    return TRUE;
}

BOOL EXPENTRY installHook( HAB habAB, PHOOKDATA phd )
{
    if( DosQueryModuleHandle( DLL_NAME, &hm ) != 0 )
        return FALSE;

    hab = habAB;
    WinSetHook( hab, NULLHANDLE, HK_INPUT, ( PFN )inputHook, hm );
    WinSetHook( hab, NULLHANDLE, HK_ACCEL, ( PFN )accelHook, hm );
    WinSetHook( hab, NULLHANDLE, HK_SENDMSG, ( PFN )sendMsgHook, hm );
    WinSetHook( hab, NULLHANDLE, HK_DESTROYWINDOW, (PFN)destroyWindowHook, hm );

    hwndKime = phd->hwndKime;
    hwndHIA = phd->hwndHIA;
    hwndIB = phd->hwndIB;
    hwndKHS = phd->hwndKHS;

    hwndCurrentInput = NULLHANDLE;

    oldKimeWndProc = WinSubclassWindow( hwndKime, newKimeWndProc );

    WinSendMsg( hwndHIA, HIAM_SETHANJAKEYCHECKPROC, MPFROMP( isHanjaKey ), 0 );
    WinSendMsg( hwndHIA, HIAM_SETSPECIALCHARKEYCHECKPROC, MPFROMP( isSpecialCharKey ), 0 );

    return TRUE;
}

VOID EXPENTRY uninstallHook( VOID )
{
    WinSubclassWindow( hwndKime, oldKimeWndProc );

    WinReleaseHook( hab, NULLHANDLE, HK_INPUT, ( PFN )inputHook, hm );
    WinReleaseHook( hab, NULLHANDLE, HK_ACCEL, ( PFN )accelHook, hm );
    WinReleaseHook( hab, NULLHANDLE, HK_SENDMSG, ( PFN )sendMsgHook, hm );
    WinReleaseHook( hab, NULLHANDLE, HK_DESTROYWINDOW, (PFN)destroyWindowHook, hm );

    WinBroadcastMsg(HWND_DESKTOP, WM_NULL, 0, 0, BMSG_FRAMEONLY | BMSG_POST);
}

BOOL isDblJaum( UCHAR uch )
{
    return (( uch == 'k' ) || ( uch == 'u' ) || ( uch == ';' ) ||
            ( uch == 'n' ) || ( uch == 'l' ));
}

BOOL isZtelnet( HWND hwnd )
{
    UCHAR szClassName[ 256 ];

    WinQueryClassName( hwnd, sizeof( szClassName ), szClassName );

    return ( strcmp( szClassName, WC_ZTELNET ) == 0 );
}

/*
BOOL isKimeProcess( HWND hwnd )
{
    PID pidKime, pidHwnd;

    WinQueryWindowProcess( hwndKime, &pidKime, NULL );
    WinQueryWindowProcess( hwnd, &pidHwnd, NULL );

    return( pidKime == pidHwnd );
}
*/

UCHAR *findKbdConv( UCHAR uch )
{
    int i;

    for( i = 0; ( kbdConvTable[ i ][ 0 ] != 0 ) &&
                ( kbdConvTable[ i ][ 0 ] != uch ); i ++ );

    return (( kbdConvTable[ i ][ 0 ] == 0 ) ? NULL : &kbdConvTable[ i ][ 0 ]);
}


USHORT kbdKeyTranslate( PQMSG pQmsg )
{
    USHORT  fsFlags = SHORT1FROMMP( pQmsg->mp1 );
    //UCHAR   ucRepeat = CHAR3FROMMP( pQmsg->mp1 );
    UCHAR   ucScancode = CHAR4FROMMP( pQmsg->mp1 );
    USHORT  usCh = SHORT1FROMMP( pQmsg->mp2 );
    USHORT  usVk = SHORT2FROMMP( pQmsg->mp2 );

    if( ucScancode < 54 )
    {
        BOOL  shiftOn = ( fsFlags & KC_SHIFT ) ? TRUE : FALSE;
        UCHAR uch = kbdKeyTransTable[ ucScancode ][ shiftOn ];

        if( uch != 0 )
        {
            uch = ( shiftOn ^ isCapsLockOn()) ? toupper( uch ) : tolower( uch );

            usCh = MAKEUSHORT( uch, 0 );

            //pQmsg->mp1 = MPFROMSH2CH( fsFlags, ucRepeat, ucScancode );
            pQmsg->mp2 = MPFROM2SHORT( usCh, usVk );
        }
    }

    return usCh;
}

BOOL isCapsLockOn( VOID )
{
    BYTE keyState[ 256 ];

    WinSetKeyboardStateTable( HWND_DESKTOP, keyState, FALSE );

    return ( keyState[ VK_CAPSLOCK ] & 0x01 );
}

VOID sendCharToWnd( HANCHAR hch )
{
    if( ISHCH( hch ))
    {
        hch = hch_sy2ks( hch );
        hch = MAKESHORT( HIBYTE( hch ), LOBYTE( hch ));
    }

    WinPostMsg( hwndCurrentInput, WM_CHAR,
                MPFROMSH2CH( KC_CHAR, 1, 0 ),
                MPFROM2SHORT( hch, 0 ));
}

#define KIMEM_CALLHANJAINPUT    ( WM_USER + 1000 )

MRESULT EXPENTRY newKimeWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    if( msg == KIMEM_CALLHANJAINPUT )
    {
        initKimeStatus( hwndCurrentInput, FALSE );

        WinSendMsg( hwndHIA, WM_CHAR, MPFROMSH2CH( KC_LONEKEY | KC_SCANCODE, 0, 0x5B ), 0 );

        WinSetFocus( HWND_DESKTOP, hwndCurrentInput );

        return 0;
    }

    if( msg == WM_CONTROL )
    {
        switch (SHORT1FROMMP(mp1))
        {
            case ID_HIA:
                switch (SHORT2FROMMP(mp1))
                {
                    case HIAN_INSERTHCH:
                        if (!(SHORT1FROMMP(mp2) & 0x8000))
                        {
                            sendCharToWnd( SHORT1FROMMP( mp2 ));

                            setInputBoxHch( 0 );
                            showInputBox( FALSE );
                        }
                        break;

                    case HIAN_COMPO_BEGIN:
                        hchComposing = SHORT2FROMMP(mp2);

                        setInputBoxHch( hchComposing );
                        showInputBox( TRUE );
                        break;

                    case HIAN_COMPO_STEP:
                        hchComposing = SHORT2FROMMP(mp2);

                        setInputBoxHch( hchComposing );
                        break;

                    case HIAN_COMPO_STEPBACK:
                        hchComposing = SHORT2FROMMP(mp2);

                        setInputBoxHch( hchComposing );
                        break;

                    case HIAN_COMPO_CANCEL:
                        setInputBoxHch( 0 );
                        showInputBox( FALSE );
                        hchComposing = 0;
                        break;

                    case HIAN_COMPO_COMPLETE:
                        sendCharToWnd( SHORT1FROMMP( mp2 ));

                        setInputBoxHch( 0 );
                        showInputBox( FALSE );
                        hchComposing = 0;
                        break;

                    case HIAN_HANMODECHANGED:
                        WinSendMsg( hwndKHS, KHSM_SETHANSTATUS, MPFROMHWND( hwndCurrentInput ), MPFROMLONG( LONGFROMMP( mp2 ) == HCH_HAN ));
                        WinSendMsg( hwndKime, KIMEM_SETHAN, MPFROMLONG( LONGFROMMP( mp2 ) == HCH_HAN ), 0 );
                        break;

                    case HIAN_KBDTYPECHANGED:
                        kimeOpt.kbdLayout = LONGFROMMR( mp2 );
                        break;

                    case HIAN_INSERTMODECHANGED:
                        break;

                    case HIAN_HGHJCONVERT:
                        WinSendMsg( hwndHIA, WM_CHAR, 0, 0 ); // call special char input

                        WinSetFocus( HWND_DESKTOP, hwndCurrentInput);
                        break;

                } // note switch end
                break;
        } // control switch end

        return 0;
    }

    return oldKimeWndProc( hwnd, msg, mp1, mp2 );
}

BOOL ztelnetAccelHook( PQMSG pQmsg )
{
    if( pQmsg->msg == WM_CHAR )
    {
        USHORT  fsFlags = SHORT1FROMMP( pQmsg->mp1 );
        //UCHAR   ucRepeat = CHAR3FROMMP( pQmsg->mp1 );
        //UCHAR   ucScancode = CHAR4FROMMP( pQmsg->mp1 );
        USHORT  usCh = SHORT1FROMMP( pQmsg->mp2 );
        USHORT  usVk = SHORT2FROMMP( pQmsg->mp2 );

        if( fsFlags & KC_KEYUP )
            return FALSE;

        if(( fsFlags & ( KC_SHIFT | KC_CTRL )) && ( usVk == VK_SPACE ))
        {
            WinSendMsg( hwndKime, KIMEM_CHANGEHAN, 0, 0 );
            WinSendMsg( hwndKHS, KHSM_CHANGEHANSTATUS, MPFROMHWND( pQmsg->hwnd ), 0 );
        }
        else if( !( fsFlags & KC_ALT ) && ( usVk == VK_F3 ))
        {
            WinSendMsg( hwndKime, KIMEM_CHANGEIM, 0, 0 );
            WinSendMsg( hwndKHS, KHSM_CHANGEIMSTATUS, MPFROMHWND( pQmsg->hwnd ), 0 );
        }
        else if( LONGFROMMR( WinSendMsg( hwndKime, KIMEM_QUERYHAN, 0, 0 ))
                 && ( !( fsFlags & ( KC_CTRL | KC_ALT ))))
        {
            UCHAR uch;
            UCHAR *kbdConv;
            BOOL  shiftOn;

            usCh = kbdKeyTranslate( pQmsg );
            uch = tolower( LOUCHAR( usCh ));
            if( fsFlags & KC_SHIFT )
                uch = toupper( uch );
            else if( kimeOpt.patchChat && prevHanInput &&
                     LONGFROMMR( WinSendMsg( hwndKime, KIMEM_QUERYIM, 0, 0 )) &&
                     ( usVk == VK_SPACE ))
                     WinSendMsg( pQmsg->hwnd, pQmsg->msg, pQmsg->mp1, pQmsg->mp2 );

            shiftOn = FALSE;
            if( isDblJaum( uch ))
            {
                if( dblJaumPressed )
                {
                    if( uchPrevDbl == uch )
                    {
                        if( kimeOpt.patch3bul )
                            WinSendMsg( pQmsg->hwnd, WM_CHAR,
                                        MPFROMSH2CH( fsFlags | KC_VIRTUALKEY, 0, 0 ),
                                        MPFROM2SHORT( 0, VK_BACKSPACE ));

                        uchPrevDbl = 0;
                        dblJaumPressed = FALSE;
                        shiftOn = TRUE;
                    }
                    else
                        uchPrevDbl = uch;
                }
                else
                {
                    uchPrevDbl = uch;
                    dblJaumPressed = TRUE;
                }
            }
            else
                dblJaumPressed = FALSE;

            if(( kbdConv = findKbdConv( uch )) != NULL )
            {
                fsFlags &= ~KC_SHIFT;
                if( kbdConv[ 3 ] || shiftOn )
                    fsFlags |= KC_SHIFT;

                usCh = MAKEUSHORT( kbdConv[ 1 ], 0 );
                if( kbdConv[ 2 ])
                {
                    if( kimeOpt.patch3bul )
                        WinSendMsg( pQmsg->hwnd, WM_CHAR,
                                    MPFROMSH2CH( fsFlags, 0, 0 ),
                                    MPFROM2SHORT( usCh, 0 ));

                    usCh = MAKEUSHORT( kbdConv[ 2 ], 0 );
                }

                if( usCh )
                {
                    if( kimeOpt.patch3bul )
                    {
                        //pQmsg->mp1 = MPFROMSH2CH( fsFlags, ucRepeat, ucScancode );
                        //pQmsg->mp2 = MPFROM2SHORT( usCh, usVk );

                        pQmsg->mp1 = MPFROMSH2CH( fsFlags, 0, 0 );
                        pQmsg->mp2 = MPFROM2SHORT( usCh, 0 );
                    }
                }
            }

            prevHanInput = ( strchr( SBCS_CHARS, SHORT1FROMMP( pQmsg->mp2 )) == NULL ) &&
                           !( fsFlags & KC_VIRTUALKEY );
        }
    }

    return FALSE;
}

BOOL kimeAccelHook( PQMSG pQmsg )
{
    if( pQmsg->msg == WM_CHAR || pQmsg->msg == WM_CHAR_SPECIAL)
    {
        static BOOL fromHook = FALSE;

        USHORT  fsFlags = SHORT1FROMMP( pQmsg->mp1 );
        UCHAR   ucRepeat = CHAR3FROMMP( pQmsg->mp1 );
        UCHAR   ucScancode = CHAR4FROMMP( pQmsg->mp1 );
        USHORT  usCh = SHORT1FROMMP( pQmsg->mp2 );
        USHORT  usVk = SHORT2FROMMP( pQmsg->mp2 );

        ULONG flHIAState;
        BOOL  hanIn;
        BOOL  consumed;
        BOOL  callHanja;
        //BOOL  patched;

        if( queryRunningHCHLB())
        {
            WinSendMsg( hwndHIA, HIAM_CHAR, pQmsg->mp1, pQmsg->mp2 );
            return TRUE;
        }

        if( fromHook )
        {
            fromHook = FALSE;

            return FALSE;
        }

        if(( pQmsg->mp1 == 0 ) && ( pQmsg->mp2 == 0 ))
        {
            fromHook = TRUE;

            return TRUE;
        }

        //patched = FALSE;

        if(( fsFlags & KC_VIRTUALKEY ) &&
           ( usVk == VK_PAGEDOWN + 0x90 ) &&
           ( ucScancode = 0x96 ))
        {
            usVk = VK_PAGEDOWN;
            ucScancode = 0x67;

            pQmsg->mp1 = MPFROMSH2CH( fsFlags, ucRepeat, ucScancode );
            pQmsg->mp2 = MPFROM2SHORT( usCh, usVk );

            //patched = TRUE;
        }

        if(( fsFlags & KC_SCANCODE ) && ( ucScancode == 0x2B ) && ( fsFlags & KC_INVALIDCHAR ))
        {
            fsFlags &= ~KC_INVALIDCHAR;
            fsFlags |= KC_CHAR;

            usCh = 0x5C;

            pQmsg->mp1 = MPFROMSH2CH( fsFlags, ucRepeat, ucScancode );
            pQmsg->mp2 = MPFROM2SHORT( usCh, usVk );

            //patched = TRUE;
        }

        callHanja = isHanjaKey( fsFlags, ucScancode, usVk, usCh );
        if((( fsFlags & KC_KEYUP ) ||
            (( fsFlags & 0x0FFF ) == KC_SCANCODE ) ||
            !( fsFlags & KC_SCANCODE )) && !callHanja )
            return FALSE;

#ifdef FOCUS_ON
        dprintf(("inputFocusChanged %d, hwndCurrentInput %04X\n",
                 inputFocusChanged, hwndCurrentInput ));

        if( inputFocusChanged || ( hwndCurrentInput != pQmsg->hwnd ))
        {
            inputFocusChanged = FALSE;

            hwndCurrentInput = pQmsg->hwnd;

            supportDBCS = ( BOOL )WinSendMsg( hwndKHS, KHSM_QUERYHANSTATUS, MPFROMHWND( hwndCurrentInput ), 0 );
            exception = checkExceptWindow( hwndCurrentInput );
        }
#else
        dprintf(("hwndCurrentInput %04X\n", hwndCurrentInput ));

        if( hwndCurrentInput != pQmsg->hwnd )
        {
            hwndCurrentInput = pQmsg->hwnd;

            initKimeStatus( hwndCurrentInput, FALSE );

            supportDBCS = ( BOOL )WinSendMsg( hwndKHS, KHSM_QUERYHANSTATUS, MPFROMHWND( hwndCurrentInput ), 0 );
            exception = checkExceptWindow( hwndCurrentInput );
        }
#endif

        dprintf(("hwndCurrentInput %04X, supportDBCS %d, exception %d\n",
                  hwndCurrentInput, supportDBCS, exception ));

        if(( fsFlags & ( KC_CTRL | KC_SHIFT )) &&
           (( fsFlags & KC_VIRTUALKEY ) && ( usVk == VK_SPACE )))
            supportDBCS = checkDBCSSupport( hwndCurrentInput );

        if( !hwndCurrentInput || !supportDBCS || exception )
            return FALSE;

        if(( fsFlags & ( KC_CTRL | KC_SHIFT )) &&
           (( fsFlags & KC_VIRTUALKEY ) && ( usVk == VK_SPACE )))
            return ( BOOL )WinSendMsg( hwndHIA, WM_CHAR, pQmsg->mp1, pQmsg->mp2 );

        flHIAState = (ULONG) WinSendMsg( hwndHIA, HIAM_QUERYSTATE, 0L, 0L );
        hanIn = flHIAState & HIAST_HANMODE;

        if( !HIUSHORT( flHIAState ) &&
            ( fsFlags & ( KC_VIRTUALKEY | KC_CTRL | KC_ALT )) &&
            !callHanja )
            return FALSE;

        if( hanIn /*|| patched */ )
        {
            //MPARAM mp2;

            consumed = FALSE;

            if( callHanja )
            {
                WinPostMsg( hwndKime, KIMEM_CALLHANJAINPUT, 0, 0 );
                consumed = TRUE;
            }
            else if((( fsFlags & KC_CHAR ) || (( fsFlags & KC_VIRTUALKEY ) && (( usVk == VK_ESC ) || ( usVk == VK_SHIFT )))) &&
                    !(( fsFlags & KC_VIRTUALKEY ) && (( usVk == VK_TAB )/* || ( usVk == VK_SPACE )*/)))
            {

                //mp2 = pQmsg->mp2;
                //kbdKeyTranslate( pQmsg );
                consumed = (BOOL)WinSendMsg( hwndHIA, WM_CHAR, pQmsg->mp1, pQmsg->mp2 );
                //pQmsg->mp2 = mp2;

            }
            else if( !isHanjaKey( fsFlags | KC_LONEKEY, ucScancode, usVk, usCh ))
                WinSendMsg( hwndHIA, HIAM_COMPLETEHCH, 0, 0 );

            if( !consumed )
            {
                if( fsFlags & ( KC_CTRL | KC_ALT ))
                    return FALSE;

                if(( fsFlags & KC_VIRTUALKEY ) && ( usVk >= VK_F1 ) && ( usVk <= VK_F24 ))
                    return FALSE;

                if( pQmsg->msg == WM_CHAR )
                {
                    WinPostMsg( pQmsg->hwnd, pQmsg->msg, 0, 0 );

#if 0
                    // IME do as the following.
                    if( HIUSHORT( flHIAState ) && ( fsFlags & KC_CHAR ) && ( usCh == ' ' ))
                    {
                        pQmsg->mp1 = MPFROMSH2CH( KC_CHAR, ucRepeat, 0 );
                        pQmsg->mp2 = MPFROM2SHORT( usCh, 0 );
                    }
#endif
                }
                WinPostMsg( pQmsg->hwnd, pQmsg->msg, pQmsg->mp1, pQmsg->mp2 );
            }

            return TRUE;
        }
    }

    return FALSE;
}

VOID ztelnetSendMsgHook( PSMHSTRUCT psmh )
{
    if( psmh->msg == WM_SETFOCUS )
    {
        //HWND hwnd = HWNDFROMMP( psmh->mp1 );
        BOOL focus = SHORT1FROMMP( psmh->mp2 );

        if( focus )
        {
            initKimeStatus( psmh->hwnd, TRUE );
            //WinSetWindowPos( hwndKime, HWND_TOP, 0, 0, 0, 0, SWP_SHOW | SWP_ZORDER );
            //WinInvalidateRect( hwndKime, NULL, TRUE );
        }
    }
}

VOID kimeSendMsgHook( PSMHSTRUCT psmh )
{
    if( psmh->msg == WM_SETFOCUS )
    {
        HWND hwnd = HWNDFROMMP( psmh->mp1 );
        BOOL focus = SHORT1FROMMP( psmh->mp2 );

        if( focus )
        {
            if( !isHCHLB( hwnd ))
            {
                WinSendMsg( hwndHIA, queryRunningHCHLB() ? HIAM_DESTROYHCHLB : HIAM_COMPLETEHCH, 0, 0 );
#ifdef FOCUS_ON
                if( hwndCurrentInput != psmh->hwnd )
                {
                    inputFocusChanged = TRUE;
                    hwndCurrentInput = psmh->hwnd;
                    initKimeStatus( hwndCurrentInput, FALSE );
                }
#endif
            }
        }
    }
}

VOID initKimeStatus( HWND hwnd, BOOL ztelnet )
{
    BOOL hanStatus;
    BOOL imStatus;

    if( !findWnd( hwnd ))
        addWnd( hwnd );

    hanStatus = ( BOOL )WinSendMsg( hwndKHS, KHSM_QUERYHANSTATUS, MPFROMHWND( hwnd ), 0 );
    WinSendMsg( hwndKime, KIMEM_SETHAN, MPFROMLONG( hanStatus ), 0 );
    if( !ztelnet )
        WinSendMsg( hwndHIA, HIAM_SETHANMODE, MPFROMLONG( hanStatus ? HCH_HAN : HCH_ENG ), 0 );

    imStatus = ( BOOL )WinSendMsg( hwndKHS, KHSM_QUERYIMSTATUS, MPFROMHWND( hwnd ), 0 );
    WinSendMsg( hwndKime, KIMEM_SETIM, MPFROMLONG( imStatus ), 0 );
}

BOOL isHanjaKey( USHORT fsFlags, UCHAR ucScancode, USHORT usVk, USHORT usCh )
{
    if(( fsFlags & KC_LONEKEY ) && ( fsFlags & KC_SCANCODE ) &&
       ( ucScancode == 0x5B ) && !( fsFlags & ( KC_ALT | KC_SHIFT )))         // if right CTRL
        return TRUE;

    return FALSE;
}

BOOL isSpecialCharKey( USHORT fsFlags, UCHAR ucScancode, USHORT usVk, USHORT usCh )
{
    if(( fsFlags == 0 ) && ( ucScancode == 0 ) && ( usVk == 0 ) && ( usCh == 0 ))
        return TRUE;

    return FALSE;
}

BOOL isHCHLB( HWND hwnd )
{
    static CHAR szBuffer[ 256 ];

    WinQueryClassName( hwnd, sizeof( szBuffer ), szBuffer );
    if( strcmp( szBuffer, WC_HCHLB ) == 0 )
        return TRUE;

    WinQueryClassName( WinQueryWindow( hwnd, QW_OWNER ), sizeof( szBuffer ), szBuffer );
    return ( BOOL )( strcmp( szBuffer, WC_HCHLB ) == 0 );
}

#ifdef DEBUG
VOID storeMsg( char *format, ... )
{
    static char buf[ 257 ] = { 0, };
    PVOID msg = NULL;

    va_list arg;

    va_start( arg, format );
    vsprintf( buf, format, arg );
    va_end( arg );

    DosAllocSharedMem( &msg, NULL, strlen( buf ) + 1, fALLOCSHR );
    strcpy( msg, buf );

    WinSendMsg( hwndKHS, KHSM_STOREMSG, ( MPARAM )msg, 0 );

    DosFreeMem( msg );
}
#endif

