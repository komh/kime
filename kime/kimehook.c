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

#ifndef HK_ACCEL
#define HK_ACCEL    17
#endif

#define DLL_NAME    "KIMEHOOK"

#define WC_ZTELNET  "ClientWindowClass"

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

static PFNWP oldKimeWndProc = NULL;
static VOID sendCharToWnd( HANCHAR hch );
static MRESULT EXPENTRY newKimeWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );

#ifdef DEBUG
#define ALARM   WinAlarm( HWND_DESKTOP, WA_NOTE )
#else
#define ALARM
#endif

#define showInputBox( flShow ) \
    ( WinSendMsg( hwndIB, IBM_SHOWINPUTBOX, MPFROMHWND( hwndCurrentInput ), MPFROMLONG( flShow )))

#define findWnd( hwnd ) (( BOOL )WinSendMsg( hwndKHS, KHSM_FINDWND, MPFROMHWND( hwnd ), 0 ))
#define addWnd( hwnd ) ( WinSendMsg( hwndKHS, KHSM_ADDWND, MPFROMHWND( hwnd ), 0 ))
#define delWnd( hwnd ) ( WinSendMsg( hwndKHS, KHSM_DELWND, MPFROMHWND( hwnd ), 0 ))
#define checkDBCSSupport( hwnd ) ( LONGFROMMR( WinSendMsg( hwndKHS, KHSM_CHECKDBCSSUPPORT, MPFROMHWND( hwnd ), 0 )))
#define checkExceptWindow( hwnd ) ( LONGFROMMR( WinSendMsg( hwndKHS, KHSM_ISEXCEPTWINDOW, MPFROMHWND( hwnd ), 0 )))

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
static VOID initKimeStatus( HWND hwnd );

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
    WinSetHook( hab, NULLHANDLE, HK_ACCEL, ( PFN )accelHook, hm );
    WinSetHook( hab, NULLHANDLE, HK_SENDMSG, ( PFN )sendMsgHook, hm );
    WinSetHook( hab, NULLHANDLE, HK_DESTROYWINDOW, (PFN)destroyWindowHook, hm );

    hwndKime = phd->hwndKime;
    hwndHIA = phd->hwndHIA;
    hwndIB = phd->hwndIB;
    hwndKHS = phd->hwndKHS;

    hwndCurrentInput = NULLHANDLE;

    oldKimeWndProc = WinSubclassWindow( hwndKime, newKimeWndProc );

    return TRUE;
}

VOID EXPENTRY uninstallHook( VOID )
{
    WinSubclassWindow( hwndKime, oldKimeWndProc );

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

MRESULT EXPENTRY newKimeWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
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
                            WinSendMsg( hwndIB, IBM_SETHANCHAR, 0, 0 );
                            showInputBox( FALSE );

                            sendCharToWnd( SHORT1FROMMP( mp2 ));
                        }
                        break;

                    case HIAN_COMPO_BEGIN:
                        hchComposing = SHORT2FROMMP(mp2);

                        WinSendMsg( hwndIB, IBM_SETHANCHAR, MPFROMSHORT( hchComposing ), 0 );
                        showInputBox( TRUE );
                        break;

                    case HIAN_COMPO_STEP:
                        hchComposing = SHORT2FROMMP(mp2);

                        WinSendMsg( hwndIB, IBM_SETHANCHAR, MPFROMSHORT( hchComposing ), 0 );
                        break;

                    case HIAN_COMPO_STEPBACK:
                        hchComposing = SHORT2FROMMP(mp2);

                        WinSendMsg( hwndIB, IBM_SETHANCHAR, MPFROMSHORT( hchComposing ), 0 );
                        break;

                    case HIAN_COMPO_CANCEL:
                        hchComposing = 0;

                        WinSendMsg( hwndIB, IBM_SETHANCHAR, MPFROMSHORT( hchComposing ), 0 );
                        showInputBox( FALSE );
                        break;

                    case HIAN_COMPO_COMPLETE:
                        WinSendMsg( hwndIB, IBM_SETHANCHAR, 0, 0 );
                        showInputBox( FALSE );

                        sendCharToWnd( SHORT1FROMMP( mp2 ));

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

/*
                    case HIAN_HGHJCONVERT:
                    {
                        HANCHAR hch;

                        if(( hchComposing >= 0xd900 ) && ( hchComposing <= 0xde00 ))
                            break;

                        hch = hjselDlg( HWND_DESKTOP, hwnd, NULLHANDLE, hchComposing );
                        if( hch != HANCHAR_SINGLE_SPACE )
                            sendCharToWnd( hch );

                        hchComposing = 0;
                        break;
                    }
*/

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
    if( pQmsg->msg == WM_CHAR )
    {
        USHORT  fsFlags = SHORT1FROMMP( pQmsg->mp1 );
        UCHAR   ucRepeat = CHAR3FROMMP( pQmsg->mp1 );
        //UCHAR   ucScancode = CHAR4FROMMP( pQmsg->mp1 );
        USHORT  usCh = SHORT1FROMMP( pQmsg->mp2 );
        USHORT  usVk = SHORT2FROMMP( pQmsg->mp2 );

        ULONG flHIAState;
        BOOL hanIn;
        BOOL consumed;

        if(( fsFlags & KC_KEYUP ) || (( fsFlags & 0x0FFF ) == KC_SCANCODE ) ||
           !( fsFlags & KC_SCANCODE ))
            return FALSE;

        if(( fsFlags & KC_VIRTUALKEY ) && ( usVk == VK_PAGEDOWN + 0x90 ))
        {
            usVk = VK_PAGEDOWN;

            pQmsg->mp2 = MPFROM2SHORT( usCh, usVk );
        }

        if( hwndCurrentInput != pQmsg->hwnd )
        {
            hwndCurrentInput = pQmsg->hwnd;
            supportDBCS = checkDBCSSupport( hwndCurrentInput );
            exception = checkExceptWindow( hwndCurrentInput );

            if( supportDBCS && !exception )
                initKimeStatus( hwndCurrentInput );
        }

        flHIAState = (ULONG) WinSendMsg( hwndHIA, HIAM_QUERYSTATE, 0L, 0L );
        hanIn = flHIAState & HIAST_HANMODE;

        if(( hanIn || (( usVk == VK_SPACE ) && ( fsFlags & KC_VIRTUALKEY ))) &&
           supportDBCS  && !exception )
        {
            MPARAM mp2;

#ifdef DEBUG
            WinSendMsg( hwndKHS, KHSM_STOREKEYINFO, pQmsg->mp1, pQmsg->mp2 );
#endif
            consumed = FALSE;

            if(( usVk != VK_INSERT ) && ( usVk != VK_TAB ))
            {
                mp2 = pQmsg->mp2;
                kbdKeyTranslate( pQmsg );
                consumed = (BOOL)WinSendMsg( hwndHIA, WM_CHAR, pQmsg->mp1, pQmsg->mp2 );
                pQmsg->mp2 = mp2;
            }
            else WinSendMsg( hwndHIA, HIAM_COMPLETEHCH, 0, 0 );

            if( !consumed )
            {
                if( fsFlags & ( KC_CTRL | KC_ALT ))
                    return FALSE;

                WinPostMsg( pQmsg->hwnd, WM_CHAR,
                            MPFROMSH2CH( fsFlags & ~KC_SCANCODE, ucRepeat, 0 ),
                            pQmsg->mp2 );
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
            initKimeStatus( psmh->hwnd );
            //WinSetWindowPos( hwndKime, HWND_TOP, 0, 0, 0, 0, SWP_SHOW | SWP_ZORDER );
            //WinInvalidateRect( hwndKime, NULL, TRUE );
        }
    }
}


VOID kimeSendMsgHook( PSMHSTRUCT psmh )
{
    if( psmh->msg == WM_SETFOCUS )
    {
        //HWND hwnd = HWNDFROMMP( psmh->mp1 );
        BOOL focus = SHORT1FROMMP( psmh->mp2 );

        if( focus )
            WinSendMsg( hwndHIA, HIAM_COMPLETEHCH, 0, 0 );
    }
}

VOID initKimeStatus( HWND hwnd )
{
    BOOL hanStatus;
    BOOL imStatus;

    if( !findWnd( hwnd ))
        addWnd( hwnd );

    hanStatus = ( BOOL )WinSendMsg( hwndKHS, KHSM_QUERYHANSTATUS, MPFROMHWND( hwnd ), 0 );
    WinSendMsg( hwndKime, KIMEM_SETHAN, MPFROMLONG( hanStatus ), 0 );
    WinSendMsg( hwndHIA, HIAM_SETHANMODE, MPFROMLONG( hanStatus ? HCH_HAN : HCH_ENG ), 0 );

    imStatus = ( BOOL )WinSendMsg( hwndKHS, KHSM_QUERYIMSTATUS, MPFROMHWND( hwnd ), 0 );
    WinSendMsg( hwndKime, KIMEM_SETIM, MPFROMLONG( imStatus ), 0 );
}
