#ifndef __EXCEPT_H__
#define __EXCEPT_H__

#include <os2.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

char *exceptCreateListBuf( const char *filename );
void exceptDestroyListBuf( char *buf );
int exceptQueryCount( char *buf );
void exceptQueryName( char *buf, int index, char *name, int len );
BOOL exceptFindName( char *buf, char *name );

#ifdef __cplusplus
}
#endif

#endif
