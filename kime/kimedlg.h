#ifndef __KIMEDLG_H__
#define __KIMEDLG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define IDG_PATCHGROUP  100
#define IDB_3BUL        101
#define IDB_CHATLINE    102

#define IDG_KBDLAYOUT   200
#define IDB_KBD2        201
#define IDB_KBD390      202
#define IDB_KBD3F       203

#define KL_KBD2     0
#define KL_KBD390   1
#define KL_KBD3F    2

typedef struct tagOPTDLGPARAM {
    ULONG   kbdLayout;
    BOOL    patch3bul;
    BOOL    patchChat;
} OPTDLGPARAM, *POPTDLGPARAM;

MRESULT EXPENTRY optDlgProc( HWND, ULONG, MPARAM, MPARAM );

#ifdef __cplusplus
}
#endif

#endif
