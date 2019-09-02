#ifndef __KIMEDLG_H__
#define __KIMEDLG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define IDG_KBDLAYOUT   200
#define IDB_KBD2        201
#define IDB_KBD390      202
#define IDB_KBD3F       203
#define IDHS_KBD2       211
#define IDHS_KBD390     212
#define IDHS_KBD3F      213

#define IDG_FONTASSOCIATION 300
#define IDLB_FONTLIST       301
#define IDB_FONTAPPLY       302
#define IDHS_CURRENT        303
#define IDHS_CURRENTFONT    304

#define KL_KBD2     0
#define KL_KBD390   1
#define KL_KBD3F    2

typedef struct tagOPTDLGPARAM {
    ULONG   kbdLayout;
    BOOL    useOS2IME;
} OPTDLGPARAM, *POPTDLGPARAM;

MRESULT EXPENTRY optDlgProc( HWND, ULONG, MPARAM, MPARAM );

#ifdef __cplusplus
}
#endif

#endif
