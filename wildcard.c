#include <stdio.h>
#include <strings.h>

    int
wildcard( char *key, char *string )
{
    char	*p1, *p2, *star = NULL;

    p1 = key;
    p2 = string;

    do {
	switch( *p1 ) {
	case '*':
	    while ( *(p1+1) == '*' ) {
		p1++;
	    }
	    if ( *(p1+1) == '\0' ) {
		return( 1 );
	    } else {
		if (( star = strchr( p1+1, '*' )) != NULL ) {
		    *star = '\0';
		}
		if (( p2 = strstr( p2, p1+1 )) == NULL ) {
		    if ( star != NULL ) {
			*star = '*';
		    }
		    return( 0 );
		} else {
		    if ( star != NULL ) {
			*star = '*';
		    }
		    return( wildcard( p1+1, p2 ));
		}
	    }
	    break;
	default:
	    if ( *p1 != *p2 ) {
		return( 0 );
	    } else {
		p1++;
		p2++;
	    }
	    break;
	}
    } while (( *p1 != '\0' ) && ( *p2 != '\0' )); 

    if ( *p2 == '\0' ) {
	return( 1 );
    } else {
	return( 0 );
    }
}
