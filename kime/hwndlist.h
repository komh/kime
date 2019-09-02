#ifndef __HWNDLIST_H__
#define __HWNDLIST_H__

#include <os2.h>


typedef struct tagHWNDLIST HWNDLIST, *PHWNDLIST;
struct tagHWNDLIST
{
    HWNDLIST    *prev, *next;
    HWND        hwnd;
    BOOL        han;
    BOOL        line;
};

#ifdef __cplusplus
extern "C" {
#endif

HWNDLIST *hwndlistCreate( VOID );
VOID hwndlistDestroy( HWNDLIST *list );

BOOL hwndlistInsert( HWNDLIST *current, HWNDLIST *toInsert );
BOOL hwndlistDelete( HWNDLIST *current );

HWNDLIST *hwndlistSearch( HWNDLIST *list, HWND hwnd );

#ifdef __cplusplus
}
#endif

#endif
