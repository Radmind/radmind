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

struct node {
    char                *path;
    int			negative;
    struct node         *next;
};

struct node* create_node( char *path );
void free_node( struct node *node );
void free_list( struct node *head );

const EVP_MD    *md;

   struct node *
create_node( char *path )
{
    struct node         *new_node;

    new_node = (struct node *) malloc( sizeof( struct node ) );
    new_node->path = strdup( path );
    new_node->negative = 0;

    return( new_node );
}

    void
free_node( struct node *node )
{
    free( node->path );
    free( node );
}

    void
free_list( struct node *head )
{
    struct node *node;

    while ( head != NULL ) {
        node = head;
        head = head->next;
        free_node( node );
    }
}

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
    struct transcript	*tran;
    extern struct transcript	*tran_head;

    while (( c = getopt( argc, argv, "aK:sV" )) != EOF ) {
	switch( c ) {
	case 'a':
	    displayall = 1;
	    break;
	case 'K':
	    defaultkfile = 0;
	    kfile = optarg;
	    break;
	case 's':
	    server = 1;
	    break;
	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );
	case '?':
	    err++;
	    break;
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
        fprintf( stderr, "Usage: %s [ -aV ] [ -K command file ] file\n",
	    argv[ 0 ] );
        fprintf( stderr, "Usage: %s -s -K command [ -aV ] file\n",
	    argv[ 0 ] );
        exit( 2 );
    }

    /* initialize the transcripts */
    transcript_init( kfile );
    edit_path = APPLICABLE;
    outtran = stdout;

    for ( tran = tran_head; !tran->t_eof; tran = tran->t_next ) {

	while (( cmp = pathcmp( tran->t_pinfo.pi_name, pattern )) < 0 ) {
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

	    if ( !tran->t_pinfo.pi_minus ) {
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
