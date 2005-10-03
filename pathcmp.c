/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>

#include "pathcmp.h"

    int
pathcmp_case( char *p1, char *p2, int case_sensitive )
{
    if ( case_sensitive ) {
	return( pathcmp( p1, p2 ));
    } else {
	return( pathcasecmp( p1, p2 ));
    }
}

/* Just like strcmp(), but pays attention to the meaning of '/'.  */
    int 
pathcmp( char *p1, char *p2 )
{
    int		rc;

    do {
	rc = ( *p1 - *p2 );
	if ( rc != 0 ) {
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
pathcasecmp( char *p1, char *p2 )
{
    int		rc;

    do {
	rc = ( tolower( *p1 ) - tolower( *p2 ));
	if ( rc != 0 ) {
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
ischild_case( char *child, char *parent, int case_sensitive )
{
    if ( case_sensitive ) {
	return( ischild( child, parent ));
    } else {
	return( ischildcase( child, parent ));
    }
}

    int
ischild( char *child, char *parent )
{
    int		rc;
    size_t	parentlen;

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
    rc = strncmp( parent, child, parentlen );
    if (( rc == 0 ) && (( '/' == child[ parentlen ] ) ||
	    ( '\0' == child[ parentlen ] ))) {
	return( 1 );
    }
    return( 0 );
}

    int
ischildcase( char *child, char *parent )
{
    int		rc;
    size_t	parentlen;

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
    rc = strncasecmp( parent, child, parentlen );
    if (( rc == 0 ) && (( '/' == child[ parentlen ] ) ||
	    ( '\0' == child[ parentlen ] ))) {
	return( 1 );
    }
    return( 0 );
}
