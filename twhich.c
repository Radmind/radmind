/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "argcargv.h"
#include "code.h"
#include "pathcmp.h"

struct node {
    char                *path;
    struct node         *next;
};

struct node* create_node( char *path );
void free_node( struct node *node );
void free_list( struct node *head );

   struct node *
create_node( char *path )
{
    struct node         *new_node;

    new_node = (struct node *) malloc( sizeof( struct node ) );
    new_node->path = strdup( path );

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

    int		c, err = 0, len, linenum = 0, ac, defaultkfile = 1, cmp = 0;
    int		server = 0, specialfile = 0, displayall = 0, match = 0;
    extern char	*version;
    char	*kfile = _RADMIND_COMMANDFILE;
    char	*kdir = "";
    char	*pattern, *p, *tline, **av;
    char	line[ MAXPATHLEN * 2 ];
    char	tran[ MAXPATHLEN ];
    char	path[ MAXPATHLEN ];
    char            prepath[ MAXPATHLEN ] = { 0 };
    FILE	*f;
    ACAV	*acav;
    struct node	*head = NULL, *new_node, *node;

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

    if (( kdir = strdup( kfile )) == NULL ) {
	perror( "strdup failed" );
	exit( 2 );
    }
    if (( p = strrchr( kdir, '/' )) == NULL ) { 
	/* No '/' in kfile - use working directory */
	kdir = "./";
    } else {
	p++;
	*p = (char)'\0';
    }

    if (( f = fopen( kfile, "r" )) == NULL ) {
	perror( kfile );
	exit( 2 );
    }

    if (( acav = acav_alloc( )) == NULL ) {
	perror( "acav_alloc failed" );
	exit( 2 );
    }

    /* Read command file */
    while( fgets( line, sizeof( line ), f ) != NULL ) {
	linenum++;

	/* Check line length */
        len = strlen( line );
        if (( line[ len - 1 ] ) != '\n' ) {
            fprintf( stderr, "%s: line %d too long\n", kfile, linenum );
            exit( 2 );
        }

	if (( ac = acav_parse( acav, line, &av )) < 0 ) {
	    perror( "acav_parse failed" );
	    exit( 2 );
	}

	/* Skip blank lines and comments */
	if (( ac == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	/* Check for valid command file line */
	if ( ac < 2 ) {
	    fprintf( stderr,
		"%s: line %d: Invalid command line - too many arguments\n",
		kfile, linenum );
	    exit( 2 );
	}

	switch( *av[ 0 ] ) {
	case 's':
	    specialfile++;
	    if ( strcmp( decode( av[ 1 ] ), pattern ) == 0 ) {
		match++;
		printf( "special.T:\ns %s\n", av[ 1 ] );
		if ( !displayall ) {
		    goto done;
		}
	    } 
	    break;
	case 'n':
	case 'p':
	    new_node = create_node( av[ 1 ] );
	    new_node->next = head;
	    head = new_node;
	    break;
	default:
	    fprintf( stderr,
		"%s: line %d: Invalid command line - unknown type\n",
		kfile, linenum );
	    exit( 2 );
	}
    }
    /* Close kfile */
    if ( fclose( f ) != 0 ) {
	perror( kfile );
	exit( 2 );
    }

    for ( node = head; node != NULL; node = node->next ) {
	strncpy( tran, node->path, MAXPATHLEN );
	if ( server ) {
	    if ( snprintf( path, MAXPATHLEN, "%s../transcript/%s", kdir,
		    tran ) > MAXPATHLEN - 1 ) {
		fprintf( stderr, "path too long: %s%s\n", kdir, tran );
		exit( 2 );
	    }
	} else {
	    if ( snprintf( path, MAXPATHLEN, "%s%s", kdir, tran )
		    > MAXPATHLEN - 1 ) {
		fprintf( stderr, "path too long: %s%s\n", kdir, tran );
		exit( 2 );
	    }
	}
	if (( f = fopen( path, "r" )) == NULL ) {
	    perror( path );
	    exit( 2 );
	}

	linenum = 0;
	sprintf( prepath, "%s", "" );

	while( fgets( line, sizeof( line ), f ) != NULL ) {
	    linenum++;

	    /* Check line length */
	    len = strlen( line );
	    if (( line[ len - 1 ] ) != '\n' ) {
		fprintf( stderr, "%s: line %d too long\n", path, linenum );
		exit( 2 );
	    }

	    /* Save transcript line - must free later */
	    if (( tline = strdup( line )) == NULL ) {
		perror( "strdup" );
		exit( 2 );
	    }

	    if (( ac = acav_parse( acav, line, &av )) < 0 ) {
		perror( "acav_parse failed" );
		exit( 2 );
	    }

	    /* Skip blank lines, comments and transcript names */
	    if (( ac < 2 ) || ( *av[ 0 ] == '#' )) {
		continue;
	    }
	    /* Check transcript order */
	    if ( prepath != 0 ) {
		if ( pathcmp( decode( av[ 1 ] ), prepath ) < 0 ) {
		    fprintf( stderr, "%s: line %d: bad sort order\n",
				tran, linenum );
		    exit( 2 );
		}
	    }
	    len = strlen( path );
	    if ( snprintf( prepath, MAXPATHLEN, "%s", decode( av[ 1 ] ))
			>= MAXPATHLEN ) {
		fprintf( stderr, "%s: line %d: path too long\n",
			tran, linenum );
		exit( 2 );
	    }

	    cmp = strcmp( decode( av[ 1 ] ), pattern );
	    if ( cmp == 0 ) {
		match++;
		if (( *av[ 0 ] == 'f' ) || ( *av[ 0 ] == 'a' )) {
		    printf( "%s:\n+ %s", tran, tline );
		} else {
		    printf( "%s:\n%s", tran, tline );
		}
		if ( !displayall ) {
		    break;
		}
	    } else if ( cmp > 0 ) {
		/* We have passed where pattern should be - stop looking */
		break;
	    }
	    free( tline );
	}
	if ( fclose( f ) != 0 ) {
	    perror( path );
	    exit( 2 );
	}
	if ( !displayall && match ) {
	    goto done;
	}
    }

done:
    free_list( head );
    acav_free( acav );

    if ( match ) {
	exit( 0 );
    } else {
	exit( 1 );
    }
}
