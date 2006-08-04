/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <stdlib.h>
#include <ctype.h>

#include "wildcard.h"

    int
wildcard( char *wild, char *p, int sensitive )
{
    int		min, max;
    int		i, match;
    char	*tmp;

    for (;;) {
	switch ( *wild ) {
	case '*' :
	    wild++;

	    if ( *wild == '\0' ) {
		return( 1 );
	    }
	    for ( i = 0; p[ i ] != '\0'; i++ ) {
		if ( wildcard( wild, &p[ i ], sensitive )) {
		    return( 1 );
		}
	    }
	    return( 0 );

	case '<' :
	    wild++;

	    if ( ! isdigit( (int)*p )) {
		return( 0 );
	    }
	    i = atoi( p );
	    while ( isdigit( (int)*p )) p++;

	    if ( ! isdigit( (int)*wild )) {
		return( 0 );
	    }
	    min = atoi( wild );
	    while ( isdigit( (int)*wild )) wild++;

	    if ( *wild++ != '-' ) {
		return( 0 );
	    }

	    if ( ! isdigit( (int)*wild )) {
		return( 0 );
	    }
	    max = atoi( wild );
	    while ( isdigit( (int)*wild )) wild++;

	    if ( *wild++ != '>' ) {
		return( 0 );
	    }

	    if (( i < min ) || ( i > max )) {
		return( 0 );
	    }
	    break;

	case '?' :
	    wild++;
	    p++;
	    break;

	case '[' :
	    wild++;
	    match = 0;

	    while ( isalnum((int)*wild )) {
		if ( *wild == *p ) {
		    match = 1;
		    break;
		}
		wild++;
	    }
	    if ( *wild != ']' ) {
		while ( *wild ) {
		    if ( *wild == ']' ) {
			break;
		    }
		    wild++;
		}
		if ( *wild == '\0' ) {
		    return( 0 );
		}
	    }
	    p++;
	    wild++;

	    if ( match == 0 ) {
		return( 0 );
	    }
	    break;

	case '{' :
	    wild++;
	    tmp = p;
	    match = 1;

	    while ( *wild == ',' ) wild++;
	    while ( isprint((int)*wild )) {
		if ( *wild == ',' ) {
		    if ( match ) {
			break;
		    }

		    match = 1;
		    wild++;
		    p = tmp;
		}
		while ( *wild == ',' ) wild++;

		if ( *wild == '}' ) {
		    break;
		}

		if ( sensitive ) {
		    if ( *wild != *p ) {
			match = 0;
		    }
		} else {
		    if ( tolower( *wild ) != tolower( *p )) {
			match = 0;
		    }
		}
		
		if ( !match ) {
		    /* find next , or } or NUL */
		    while ( *wild ) {
			wild++;
			if ( *wild == ',' || *wild == '}' ) {
			    break;
			}
		    }
		} else {
		    wild++, p++;
		}
	    }

	    if ( !match ) {
		return( 0 );
	    }

	    /* verify remaining format */
	    if ( *wild != '}' ) {
		while ( *wild ) {
		    if ( *wild == '}' ) {
			break;
		    }
		    wild++;
		}
		if ( *wild == '\0' ) {
		    return( 0 );
		}
	    }
	    if ( *wild++ != '}' ) {
		return( 0 );
	    }

	    break;

	case '\\' :
	    wild++;
	default :
	    if ( sensitive ) {
	       if ( *wild != *p ) {
		   return( 0 );
	       }
	    } else {
	       if ( tolower(*wild) != tolower(*p) ) {
		  return( 0 );
		}
	    }
	    if ( *wild == '\0' ) {
		return( 1 );
	    }
	    wild++, p++;
	}
    }
}
