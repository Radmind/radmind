/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include "pathcmp.h"

/* Just like strcmp(), but pays attention to the meaning of '/'.  */
    int 
pathcmp( const unsigned char *p1, const unsigned char *p2 )
{
    int		rc;

    do {
	if (( rc = ( *p1 - *p2 )) != 0 ) {
#ifdef notdef
	    /*
	     * This (untested) code might make a trailing slash not matter
	     * during path comparison.  Of course, that means files with
	     * trailing slashes become legal....
	     *
	     * A better way to handle the issue of TAB'd filename completion
	     * is probably to make "lcheck" give a warning.
	     */
	    if ( *p1 == '/' ) {
		if ( *p2 == '\0' ) {
		    if ( *( p1 + 1 ) == '\0' ) {
			return( 0 );
		    }
		} else {
		    return( -1 );
		}
	    } else if ( *p2 == '/' ) {
		if ( *p1 == '\0' ) {
		    if ( *( p2 + 1 ) == '\0' ) {
			return( 0 );
		    }
		} else {
		    return( 1 );
		}
	    } else {
		return( rc );
	    }
#endif notdef

	    if (( *p2 != '\0' ) && ( *p1 == '/' )) {
		return( -1 );
	    } else if (( *p1 != '\0' ) && ( *p2 == '/' )) {
		return( 1 );
	    } else {
		return( rc );
	    }
	}
	p2++;
	p1++;
    } while ( *p1 != '\0' );

    return( 0 );
}
