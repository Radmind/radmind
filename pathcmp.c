/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <string.h>

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

    int
ischild( const unsigned char *child, const unsigned char *parent )
{
    size_t parentlen;

    if ( parent == NULL ) {
	return( 1 );
    }

    parentlen = strlen( parent );

    if ( parentlen > strlen( child )) {
	return( 0 );
    }
    if (( 1 == parentlen ) && ( '/' == *parent )) {
	return( '/' == *child );
    }
    if (( strncmp( parent, child, parentlen ) == 0 ) &&
	    (( '/' == child[ parentlen ] ) ||
	    ( '\0' == child[ parentlen ] ))) {
	return( 1 );
    }
    return( 0 );
}
