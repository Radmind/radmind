#include <sys/param.h>
#include <errno.h>
#include <stdio.h>

#include "code.h"

/*
 * Escape certain characters.  This code must match decode() *and*
 * argcargv().
 */
    char *
encode( char *line )
{
    /* static */
    static char	    buf[ 2 * MAXPATHLEN ];
    char	    *temp;    

    if ( strlen( line ) > MAXPATHLEN ) {
	fprintf( stderr, "ERROR: The path is too long\n" );
	exit( 1 );
    }

    temp = buf;
    for ( ; *line != '\0'; line++, temp++ ) {
	switch ( *line ) {
	case ' ' :
	    *temp = '\\';
	    temp++;
	    *temp = 'b';
	    break;
	case '\t' :
	    *temp = '\\';
	    temp++;
	    *temp = 't';
	    break;
	case '\n':
	    *temp = '\\';
	    temp++;
	    *temp = 'n';
	    break;
	case '\r':
	    *temp = '\\';
	    temp++;
	    *temp = 'r';
	    break;
	case '\\':
	    *temp = '\\';
	    temp++;
	    *temp = '\\';
	    break;
	default :
	    *temp = *line;
	    break;
	}
    }

    *temp = '\0';
    return( buf );
}

    char *
decode( char *line ) 
{
    /* static */
    static char     buf[ MAXPATHLEN ];
    char	    *temp;

    if ( strlen( line ) > ( 2 * MAXPATHLEN )) {
	fprintf( stderr, "ERROR:  The path name is too long\n" );
	exit( 1 );
    }

    temp = buf;

    for ( ; *line != '\0'; line++, temp++ ) {
	switch( *line ) {
	case '\\':
	    line++;
	    switch( *line ) {
	    case 'n':
		*temp = '\n';
		break;
	    case 't':
		*temp = '\t';
		break;
	    case 'b':
		*temp = ' ';
		break;
	    case 'r':
		*temp = '\r';
		break;
	    case '\\':
		*temp = '\\';
		break;
	    default:
		break;
	    }
	    break;
	default:
	    *temp = *line;
	    break;
	}
    }

    *temp = '\0';
    return( buf );
}
