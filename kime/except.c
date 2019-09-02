#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "except.h"

char *exceptCreateListBuf( const char *filename )
{
    FILE *fp;
    char *buf;
    struct stat fstat;

    if( stat( filename, &fstat ) == -1 )
        return NULL;

    fp = fopen( filename, "rb" );
    if( fp == NULL )
        return NULL;

    buf = malloc( fstat.st_size + 1 );
    fread( buf, fstat.st_size, 1, fp );
    buf[ fstat.st_size ] = 0;

    fclose( fp );

    return buf;
}

void exceptDestroyListBuf( char *buf )
{
    free( buf );
}

int exceptQueryCount( char *buf )
{
    char *buffer = buf;
    int  count;

    if( buffer == NULL )
        return 0;

    for( count = 0; *buffer != 0; buffer++ )
    {
        if( *buffer == '\n' )
            count++;
    }

    buffer--;
    if(( *buffer != '\n' ) && ( *buffer != '\r' ))
        count++;

    return count;

}

void exceptQueryName( char *buf, int index, char *name, int len )
{
    char *buffer = buf;
    char *qname = name;
    int i;

    *qname = 0;

    if( buffer == NULL )
        return;

    for( i = 0; ( *buffer != 0 ) && (( qname - name ) < len ) ; buffer++ )
    {
        if( i == index )
        {
            *qname++ = *buffer;
        }

        if( *buffer == '\n' )
            i++;
    }

    if( qname == name )
        return;

    qname --;
    while(( *qname == '\r' ) || ( *qname == '\n' ))
        qname--;

    *( qname + 1 ) = 0;
}

BOOL exceptFindName( char *buf, char *name )
{
    char bufName[ 256 ];
    int count;
    int i;

    if( buf == NULL )
        return FALSE;

    count = exceptQueryCount( buf );
    for( i = 0; i < count; i ++ )
    {
        exceptQueryName( buf, i, bufName, sizeof( bufName ) - 1 );
        if( stricmp( name, bufName ) == 0 )
            return TRUE;
    }

    return FALSE;
}

