/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "applefile.h"
#include "transcript.h"
#include "code.h"
#include "pathcmp.h"
#include "list.h"
#include "wildcard.h"

const EVP_MD    *md;

int		case_sensitive = 1;
int		tran_format = -1; 

/*
 * exit codes:
 *      0       File found
 *	1	File not found.
 *      >1     	An error occurred. 
 */

    static int
twhich( char *pattern, int displayall )
{
    struct node		*node;
    struct transcript	*tran;
    extern struct transcript	*tran_head;
    extern struct list	*exclude_list;
    int			cmp = 0, match = 0;

    /* check exclude list */
    if ( exclude_list->l_count > 0 ) {
	for ( node = list_pop_head( exclude_list ); node != NULL;
		node = list_pop_head( exclude_list )) {
	    if ( wildcard( node->n_path, pattern, case_sensitive )) {
		printf( "# Exclude\n" );
		printf( "# exclude pattern: %s\n", node->n_path );
		if ( !displayall ) {
		    goto done;
		}
	    }
	    free( node );
	}
    }

    for ( tran = tran_head; tran->t_next != NULL; tran = tran->t_next ) {

	/* Skip NULL/empty transcripts */
	if ( tran->t_eof ) {
	    continue;
	}

	while (( cmp = pathcasecmp( tran->t_pinfo.pi_name,
		pattern, case_sensitive )) < 0 ) {
	    transcript_parse( tran );
	    if ( tran->t_eof ) {
		break;
	    }
	}
	if ( tran->t_eof ) {
	    continue;
	}

	if ( cmp > 0 ) {
	    continue;
	}

	if ( cmp == 0 ) {
	    match++;
	    switch( tran->t_type ) {
	    case T_POSITIVE:
		printf( "# Positive\n" );
		break;

	    case T_NEGATIVE:
		printf( "# Negative\n" );
		break;

	    case T_SPECIAL:
		printf( "# Special\n" );
		break;

	    default:
		fprintf( stderr, "unknown transcript type\n" );
		exit( 2 );
	    }
	    printf( "# %s:\n", tran->t_kfile );

	    if ( tran->t_pinfo.pi_minus ) {
		printf( "%s:\n", tran->t_shortname );
		t_print( NULL, tran, PR_STATUS_MINUS );
	    } else {
		t_print( NULL, tran, PR_TRAN_ONLY );
	    }

	    if ( !displayall ) {
		goto done;
	    }
	}
    }

done:
    if ( match ) {
	return( 0 );
    } else {
	return( 1 );
    }
}

    int
main( int argc, char **argv )
{
    int			c, err = 0, defaultkfile = 1, rc = 0, len;
    int			server = 0, displayall = 0, recursive = 0;
    extern char		*version;
    char		*kfile = _RADMIND_COMMANDFILE;
    char		*pattern, *p;

    while (( c = getopt( argc, argv, "aIK:rsV" )) != EOF ) {
	switch( c ) {
	case 'a':
	    displayall = 1;
	    break;

	case 'K':
	    defaultkfile = 0;
	    kfile = optarg;
	    break;

	case 'I':
	    case_sensitive = 0;
	    break;

	case 'r':		/* recursively twhich all path elements */
	    recursive = 1;
	    break;

	case 's':
	    server = 1;
	    break;
	
	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );

	default:
	    err++;
	    break;
	}
    }

    if (( argc - optind ) != 1 ) {
	err++;
    }

    pattern = argv[ argc - 1 ];
    len = strlen( pattern );

    if ( server && defaultkfile ) {
	err++;
    }

    if ( err ) {
        fprintf( stderr, "Usage: %s [ -aIrV ] [ -K command file ] file\n",
	    argv[ 0 ] );
        fprintf( stderr, "Usage: %s -s -K command [ -aIrV ] file\n",
	    argv[ 0 ] );
        exit( 2 );
    }

    /* clip trailing slash */
    if ( len > 1 && pattern[ len - 1 ] == '/' ) {
	pattern[ len - 1 ] = '\0';
	len--;
    }

    /* Determine if called with relative or absolute pathing.  Path is relative
     * if it's just '.' or starts with './'.  File names that start with a '.'
     * are absolute.
     */
    if ( pattern[ 0 ] == '.' ) {
	if ( len == 1 ) {
	    tran_format = T_RELATIVE;
	} else if ( pattern[ 1 ] == '/' ) {
	    tran_format = T_RELATIVE;
	}
    } else {
	tran_format = T_ABSOLUTE;
    }

    /* initialize the transcripts */
    edit_path = APPLICABLE;
    if ( server ) {
	transcript_init( kfile, K_SERVER );
    } else {
	transcript_init( kfile, K_CLIENT );
    }
    outtran = stdout;

    if ( recursive ) {
	for ( p = pattern; *p == '/'; p++ )
	    ;
	for ( p = strchr( p, '/' ); p != NULL; p = strchr( p, '/' )) {
	    *p = '\0';
	    if ( twhich( pattern, displayall ) != 0 ) {
		printf( "# %s: not found\n", pattern );
	    }

	    *p++ = '/';
	}
    }
    rc = twhich( pattern, displayall );

    exit( rc );
}
