/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "pathcmp.h"

/* Just like strcmp(), but pays attention to the meaning of '/'.  */
    int 
pathcmp( const unsigned char *p1, const unsigned char *p2 )
{
    int		rc;

    do {
	if (( rc = ( *p1 - *p2 )) != 0 ) {
	    if (( *p2 != '\0' ) && ( *p1 == '/' )) {
		return( -1 );
	    } else if (( *p1 != '\0' ) && ( *p2 == '/' )) {
		return( 1 );
	    } else {
		return( rc );
	    }
	}
	p2++;
    } while ( *p1++ != '\0' );

    return( 0 );
}
