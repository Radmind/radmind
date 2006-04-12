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

const EVP_MD    *md;

int		case_sensitive = 1;

/*
 * exit codes:
 *      0       File found
 *	1	File not found.
 *      >1     	An error occurred. 
 */

    int
main( int argc, char **argv )
{

    int			c, err = 0, defaultkfile = 1, cmp = 0;
    int			server = 0, displayall = 0, match = 0;
    extern char		*version;
    char		*kfile = _RADMIND_COMMANDFILE;
    char		*pattern;
    struct node		*node;
    struct transcript	*tran;
    extern struct transcript	*tran_head;
    extern struct list	*special_list;

    while (( c = getopt( argc, argv, "aIK:sV" )) != EOF ) {
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

    if ( server && defaultkfile ) {
	err++;
    }

    if ( err ) {
        fprintf( stderr, "Usage: %s [ -aIV ] [ -K command file ] file\n",
	    argv[ 0 ] );
        fprintf( stderr, "Usage: %s -s -K command [ -aIV ] file\n",
	    argv[ 0 ] );
        exit( 2 );
    }

    /* initialize the transcripts */
    edit_path = APPLICABLE;
    if ( server ) {
	transcript_init( kfile, K_SERVER );
    } else {
	transcript_init( kfile, K_CLIENT );
    }
    outtran = stdout;

    /* check special list */
    if ( special_list->l_count > 0 ) {
	for ( node = list_pop_head( special_list ); node != NULL;
		node = list_pop_head( special_list )) {
	    if ( pathcasecmp( node->n_path, pattern, case_sensitive ) == 0 ) {
		printf( "# Special\n" );
		printf( "special.T:\n" );
		printf( "%s\n", node->n_path );
		free( node );
		if ( !displayall ) {
		    goto done;
		}
	    }
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
	exit( 0 );
    } else {
	exit( 1 );
    }
}
