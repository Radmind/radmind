#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>

#include "code.h"

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
