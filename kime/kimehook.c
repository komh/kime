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
#include <os2im.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "kime.h"
#include "kimehook.h"
#include "kimeconv.inc"

#include "inputbox.h"
#include "../hanlib/han.h"

#include "../hchlb/hchlbdlg.h"

#include "../im32/im32.h"
#include "../im32/toggle.h"

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

#define WC_HCHLB    "HanCharListBox"

OPTDLGPARAM kimeOpt = { KL_KBD2, TRUE, TRUE, FALSE };

static HAB hab = 0;
static HWND hwndKime = NULLHANDLE;
static HWND hwndHIA = NULLHANDLE;
static HWND hwndIB = NULLHANDLE;
static HWND hwndKHS = NULLHANDLE;
static HWND hwndCurrentInput = NULLHANDLE;

static HMODULE hm = NULLHANDLE;

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

#define ODIN_SUPPORT_IN_INPUT_HOOK

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

#define setHanStatus( hwnd, han ) \
    do { \
        WinSendMsg( hwndKHS, KHSM_SETHANSTATUS, \
                    MPFROMHWND( hwnd ), MPFROMLONG( han )); \
        WinSendMsg( hwndKime, KIMEM_SETHAN, MPFROMLONG( han ), 0 ); \
    } while( 0 )

//static BOOL isKimeProcess( HWND hwnd );
static BOOL kimeAccelHook( PQMSG pQmsg );
static VOID kimeSendMsgHook( PSMHSTRUCT psmh );
static VOID initKimeStatus( HWND hwnd );
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
#ifdef ODIN_SUPPORT_IN_INPUT_HOOK
    else if( pQmsg->msg == WM_CHAR_SPECIAL ) // for Odin
    {
        ULONG   flHIAState = (ULONG) WinSendMsg( hwndHIA, HIAM_QUERYSTATE, 0L, 0L );
        USHORT  fsFlags = SHORT1FROMMP( pQmsg->mp1 );
        //UCHAR   ucRepeat = CHAR3FROMMP( pQmsg->mp1 );
        //UCHAR   ucScancode = CHAR4FROMMP( pQmsg->mp1 );
        //USHORT  usCh = SHORT1FROMMP( pQmsg->mp2 );
        //USHORT  usVk = SHORT2FROMMP( pQmsg->mp2 );


        if( !( fsFlags & KC_KEYUP ) && HIUSHORT( flHIAState ))
        {
            WinSendMsg( hwndHIA, HIAM_COMPLETEHCH, 0, 0 );
            WinPostMsg( pQmsg->hwnd, pQmsg->msg, pQmsg->mp1, pQmsg->mp2 );

            return TRUE;
        }
    }
#endif

    return FALSE;
}

BOOL EXPENTRY accelHook( HAB hab, PQMSG pQmsg, USHORT fsOptions )
{
    return kimeAccelHook( pQmsg );
}

VOID EXPENTRY sendMsgHook( HAB hab, PSMHSTRUCT psmh, BOOL fInterTask )
{
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

    IM32Init();

    hab = habAB;
    WinSetHook( hab, NULLHANDLE, HK_INPUT, ( PFN )inputHook, hm );
    WinSetHook( hab, NULLHANDLE, HK_ACCEL, ( PFN )accelHook, hm );
    WinSetHook( hab, NULLHANDLE, HK_SENDMSG, ( PFN )sendMsgHook, hm );
    WinSetHook( hab, NULLHANDLE, HK_DESTROYWINDOW, (PFN)destroyWindowHook, hm );

    hwndKime = phd->hwndKime;
    hwndHIA = phd->hwndHIA;
    hwndIB = phd->hwndIB;
    hwndKHS = phd->hwndKHS;

    hwndCurrentInput = WinQueryFocus( HWND_DESKTOP );

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

    IM32Term();
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
#define KIMEM_INITKIMESTATUS    ( WM_USER + 1001 )

MRESULT EXPENTRY newKimeWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    if( msg == KIMEM_INITKIMESTATUS )
    {
        initKimeStatus( hwndCurrentInput );

        return 0;
    }

    if( msg == KIMEM_RELOAD )
    {
        WinSendMsg( hwndKHS, KHSM_RELOADEXCEPTFILE, 0, 0 );
        hwndCurrentInput = WinQueryFocus( HWND_DESKTOP );
        initKimeStatus( hwndCurrentInput );

        return 0;
    }

    if( msg == KIMEM_CALLHANJAINPUT )
    {
        initKimeStatus( hwndCurrentInput );

        WinSendMsg( hwndHIA, WM_CHAR, MPFROMSH2CH( KC_LONEKEY | KC_SCANCODE, 0, 0x36 ), 0 );

        WinSetFocus( HWND_DESKTOP, hwndCurrentInput );

        return 0;
    }

    if( msg == KIMEM_CHANGEHANHOOK )
    {
        if( checkExceptWindow( hwndCurrentInput ))
            return 0;

        if( kimeOpt.useOS2IME )
            toggleIMEHanEng( hwndCurrentInput );
        else if( checkDBCSSupport( hwndCurrentInput ))
            WinSendMsg( hwndHIA, HIAM_CHANGEHANMODE, 0, 0 );

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
                    case HIAN_COMPO_STEP:
                    case HIAN_COMPO_STEPBACK:
                        hchComposing = SHORT2FROMMP(mp2);

                        setInputBoxHch( hchComposing );
                        showInputBox( TRUE );
                        break;

#if 0
                    case HIAN_COMPO_STEP:
                        hchComposing = SHORT2FROMMP(mp2);

                        setInputBoxHch( hchComposing );
                        break;

                    case HIAN_COMPO_STEPBACK:
                        hchComposing = SHORT2FROMMP(mp2);

                        setInputBoxHch( hchComposing );
                        break;
#endif

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
                        setHanStatus( hwndCurrentInput, LONGFROMMP( mp2 ) == HCH_HAN );
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

BOOL kimeAccelHook( PQMSG pQmsg )
{
    if( pQmsg->msg == WM_CHAR
#ifndef ODIN_SUPPORT_IN_INPUT_HOOK
        || pQmsg->msg == WM_CHAR_SPECIAL
#endif
      )
    {
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
        dprintf(("inputFocusChanged %d, ", inputFocusChanged ));
#endif
        dprintf(("hwndCurrentInput %04X\n", hwndCurrentInput ));

        if( hwndCurrentInput != pQmsg->hwnd
#ifdef FOCUS_ON
            || inputFocusChanged
#endif
          )
        {
#ifdef FOCUS_ON
            inputFocusChanged = FALSE;
#endif
            hwndCurrentInput = pQmsg->hwnd;

            initKimeStatus( hwndCurrentInput );

            supportDBCS = checkDBCSSupport( hwndCurrentInput );
            exception = checkExceptWindow( hwndCurrentInput );
        }

        dprintf(("hwndCurrentInput %04X, supportDBCS %d, exception %d\n",
                  hwndCurrentInput, supportDBCS, exception ));

        if(( fsFlags & ( KC_CTRL | KC_SHIFT )) &&
           (( fsFlags & KC_VIRTUALKEY ) && ( usVk == VK_SPACE )))
            supportDBCS = checkDBCSSupport( hwndCurrentInput );

        if( kimeOpt.useOS2IME )
        {
            if( exception /* || !supportDBCS */)
                return FALSE;

            if((( fsFlags & ( KC_ALT | KC_CTRL | KC_SHIFT )) == KC_SHIFT ) &&
               (( fsFlags & KC_VIRTUALKEY ) && ( usVk == VK_SPACE )))
            {
                toggleIMEHanEng( hwndCurrentInput );

                return TRUE;
            }

            if( callHanja )
            {
                callIMEHanja( hwndCurrentInput );

                return TRUE;
            }

            return FALSE;
        }

        dprintf(("hwndCurrentInput %04X, supportDBCS %d, exception %d\n",
                  hwndCurrentInput, supportDBCS, exception ));

        if( !hwndCurrentInput || !supportDBCS || exception )
            return FALSE;

        if(( fsFlags & ( KC_CTRL | KC_SHIFT )) &&
           (( fsFlags & KC_VIRTUALKEY ) && ( usVk == VK_SPACE )))
            return ( BOOL )WinSendMsg( hwndHIA, WM_CHAR, pQmsg->mp1, pQmsg->mp2 );

        flHIAState = (ULONG) WinSendMsg( hwndHIA, HIAM_QUERYSTATE, 0L, 0L );
        hanIn = flHIAState & HIAST_HANMODE;

#if 0
        if( !HIUSHORT( flHIAState ) &&
            ( fsFlags & ( KC_VIRTUALKEY | KC_CTRL | KC_ALT )) &&
            !callHanja )
            return FALSE;
#endif

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
                if( !HIUSHORT( flHIAState ))
                    return FALSE;

                if( fsFlags & ( KC_CTRL | KC_ALT ))
                    return FALSE;

                if(( fsFlags & KC_VIRTUALKEY ) && (( usVk == VK_SHIFT ) || (( usVk >= VK_F1 ) && ( usVk <= VK_F24 ))))
                    return FALSE;

#ifndef ODIN_SUPPORT_IN_INPUT_HOOK
                if( pQmsg->msg == WM_CHAR )
                {
#endif

#if 0
                    // IME do as the following.
                    if( HIUSHORT( flHIAState ) && ( fsFlags & KC_CHAR ) && ( usCh == ' ' ))
                    {
                        pQmsg->mp1 = MPFROMSH2CH( KC_CHAR, ucRepeat, 0 );
                        pQmsg->mp2 = MPFROM2SHORT( usCh, 0 );
                    }
#endif

#ifndef ODIN_SUPPORT_IN_INPUT_HOOK
                }
#endif
                WinPostMsg( pQmsg->hwnd, pQmsg->msg, pQmsg->mp1, pQmsg->mp2 );
            }

            return TRUE;
        }
    }

    return FALSE;
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
                    WinPostMsg( hwndKime, KIMEM_INITKIMESTATUS, 0, 0 );
                }
#endif
            }
        }
    }
    else if( kimeOpt.useOS2IME && psmh->msg == WM_IMEREQUEST )
    {
        HWND  hwnd     = psmh->hwnd;
        ULONG ulReq    = LONGFROMMP( psmh->mp1 );
        ULONG ulStatus = LONGFROMMP( psmh->mp2 );

        if( hwnd     == hwndCurrentInput &&
            ulReq    == IMR_STATUS &&
            ulStatus == IMR_STATUS_INPUTMODE )
        {
            BOOL fHanStatus = queryIMEHanEng( hwnd );

            setHanStatus( hwnd, fHanStatus );
        }
    }
}

VOID initKimeStatus( HWND hwnd )
{
    BOOL hanStatus;

    if( WinQueryWindow( hwnd, QW_OWNER ) == hwndKime )
        return;

    if( checkExceptWindow( hwnd ) || !checkDBCSSupport( hwnd ))
    {
        WinSendMsg( hwndKime, WM_COMMAND, MPFROM2SHORT( IDM_HIDE, 0 ),
                    MPFROM2SHORT( CMDSRC_MENU, 0 ));
        return;
    }

    WinSendMsg( hwndKime, WM_COMMAND, MPFROM2SHORT( IDM_HIDE, 0xFFFF ),
                MPFROM2SHORT( CMDSRC_MENU, 0 ));

    if( !findWnd( hwnd ))
        addWnd( hwnd );

    if( !kimeOpt.useOS2IME )
    {
        hanStatus = ( BOOL )WinSendMsg( hwndKHS, KHSM_QUERYHANSTATUS,
                                        MPFROMHWND( hwnd ), 0 );
        WinSendMsg( hwndHIA, HIAM_SETHANMODE,
                    MPFROMLONG( hanStatus ? HCH_HAN : HCH_ENG ), 0 );
    }
    else
    {
        hanStatus = queryIMEHanEng( hwnd );

        setHanStatus( hwnd, hanStatus );
    }
}

BOOL isHanjaKey( USHORT fsFlags, UCHAR ucScancode, USHORT usVk, USHORT usCh )
{
    if(( fsFlags & KC_LONEKEY ) && ( fsFlags & KC_SCANCODE ) &&
       ( ucScancode == 0x36 ) && !( fsFlags & ( KC_ALT | KC_CTRL )))
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

