#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "argcargv.h"
#include "code.h"

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

    int		c, err = 0, len, linenum = 0, ac, defaultkfile = 1;
    int		server = 0, specialfile = 0, displayall = 0, match = 0;
    extern char	*version;
    char	*kfile = _RADMIND_COMMANDFILE;
    char	*kdir = "";
    char	*pattern, *p, *tline, **av;
    char	line[ MAXPATHLEN * 2 ];
    char	tran[ MAXPATHLEN ];
    char	path[ MAXPATHLEN ];
    char	serverkfile[ MAXPATHLEN ];
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

    len = strlen( kfile );
    if ( kfile[ len - 1 ] == '/' ) {
	err++;
    }

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
	if ( server ) {
	    /* Build server command file */
	    kdir = _RADMIND_SERVER_COMMAND_DIR;
	    if ( snprintf( serverkfile, MAXPATHLEN, "%s%s", kdir, kfile ) >
		    MAXPATHLEN - 1 ) {
		fprintf( stderr, "path too long: %s%s\n", kdir, kfile );
		exit( 2 );
	    }
	    kfile = serverkfile;
	} else {
	    /* No '/' in kfile - use working directory */
	    kdir = "./";
	}
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
            return( 1 );
        }

	if (( ac = acav_parse( acav, line, &av )) < 0 ) {
	    perror( "acav_parse failed" );
	    exit( 2 );
	}

	/* Skip blank lines and comments */
	if (( ac == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	switch( *av[ 0 ] ) {
	case 's':
	    specialfile++;
	    if ( strcmp( decode( av[ 1 ] ), pattern ) == 0 ) {
		match++;
		printf( "special.T: line %d\n", specialfile );
		if ( !displayall ) {
		    goto done;
		}
	    } 
	    break;
	default:
	    new_node = create_node( av[ 1 ] );
	    new_node->next = head;
	    head = new_node;
	    break;
	}
    }
    /* Close kfile */
    if ( fclose( f ) != 0 ) {
	perror( kfile );
	exit( 2 );
    }

    while ( head != NULL ) {
	node = head;
	head = node->next;
	strncpy( tran, node->path, MAXPATHLEN );
	free_node( node );
	if ( server ) {
	    if ( snprintf( path, MAXPATHLEN, "%s%s", _RADMIND_TRANSCRIPT_DIR,
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

	while( fgets( line, sizeof( line ), f ) != NULL ) {
	    linenum++;

	    /* Check line length */
	    len = strlen( line );
	    if (( line[ len - 1 ] ) != '\n' ) {
		fprintf( stderr, "%s: line %d too long\n", path, linenum );
		return( 1 );
	    }

	    if (( tline = strdup( line )) == NULL ) {
		perror( "strdup" );
		exit( 2 );
	    }

	    if (( ac = acav_parse( acav, line, &av )) < 0 ) {
		perror( "acav_parse failed" );
		exit( 2 );
	    }

	    /* Skip blank lines, comments and transcript names */
	    if (( ac <= 1 ) || ( *av[ 0 ] == '#' )) {
		continue;
	    }
	    if ( strcmp( decode( av[ 1 ] ), pattern ) == 0 ) {
		match++;
		printf( "%s: line %d:\n%s", path, linenum, tline );
		if ( !displayall ) {
		    goto closetran;
		}
	    } 
	}
closetran:
	free( tline );
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
