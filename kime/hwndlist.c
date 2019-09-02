#include <stdlib.h>

#include "hwndlist.h"

HWNDLIST *hwndlistCreate( VOID )
{
    HWNDLIST *list = NULL;

    list = malloc( sizeof( HWNDLIST ));
    if( list == NULL )
        return NULL;

    list->next = list->prev = NULL;
    list->hwnd = 0;
    list->han = FALSE;

    return list;
}

VOID hwndlistDestroy( HWNDLIST *list )
{
    HWNDLIST *start = NULL, *current = NULL, *next = NULL;

    if( list == NULL )
        return;

    for( start = list; start->prev != NULL; start = start->prev );

    for( current = start; current->next != NULL; current = next )
    {
        next = current->next;
        free( current );
    }
}

BOOL hwndlistInsert( HWNDLIST *current, HWNDLIST *toInsert )
{
    HWNDLIST *start = NULL, *end = NULL;

    if(( current == NULL ) || ( toInsert == NULL ))
        return FALSE;

    for( start = toInsert; start->prev != NULL; start = start->prev );
    for( end = toInsert; end->next != NULL; end = end->next );

    if( current->next != NULL )
    {
        current->next->prev = end;
        end->next = current->next;

    }

    current->next = start;
    start->prev = current;

    return TRUE;
}

BOOL hwndlistDelete( HWNDLIST *current )
{
    if( current == NULL )
        return FALSE;

    if( current->next != NULL )
        current->next->prev = current->prev;

    if( current->prev != NULL )
        current->prev->next = current->next;

    free( current );

    return TRUE;
}

HWNDLIST *hwndlistSearch( HWNDLIST *list, HWND hwnd )
{
    HWNDLIST *start = NULL, *current = NULL;

    for( start = list; start->prev != NULL; start = start->prev );

    for( current = start; current != NULL; current = current->next )
    {
        if( current->hwnd == hwnd )
            break;
    }

    return current;
}

