#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "argcargv.h"
#include "lifo.h"

/*
 * exit codes:
 *      0       File found
 *	1	File not found.
 *      >1     	An error occurred. 
 */


    int
main( int argc, char **argv )
{

    int		c, err = 0, len, linenum = 0, ac;
    int		specialfile = 0, displayall = 0, match = 0;
    extern char	*version;
    char	*kfile = _RADMIND_COMMANDFILE;
    char	*kdir = "";
    char	*pattern, *p, **av;
    char	line[ MAXPATHLEN * 2 ];
    char	tran[ MAXPATHLEN ];
    char	path[ MAXPATHLEN ];
    FILE	*f;
    ACAV	*acav;
    struct node	*head = NULL;

    while (( c = getopt( argc, argv, "aK:V" )) != EOF ) {
	switch( c ) {
	case 'a':
	    displayall = 1;
	    break;
	case 'K':
	    kfile = optarg;
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

    if ( err ) {
        fprintf( stderr, "Usage: %s [ -aV ] [ -K command file ] file\n",
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
	    break;
	default:
	    insert_head( av[ 1 ], &head );
	    break;
	}
    }
    if ( specialfile ) {
	insert_head( "special.T", &head );
    }
    /* Close kfile */
    if ( fclose( f ) != 0 ) {
	perror( kfile );
	exit( 2 );
    }

    while ( head != NULL ) {
	remove_head( &head, tran );
	if ( snprintf( path, MAXPATHLEN, "%s%s", kdir, tran ) > MAXPATHLEN ) {
	    fprintf( stderr, "path too long: %s%s\n", kdir, tran );
	    exit( 2 );
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

	    if (( ac = acav_parse( acav, line, &av )) < 0 ) {
		perror( "acav_parse failed" );
		exit( 2 );
	    }

	    /* Skip blank lines, comments and transcript names */
	    if (( ac <= 1 ) || ( *av[ 0 ] == '#' )) {
		continue;
	    }
	    if ( strcmp( av[ 1 ], pattern ) == 0 ) {
		printf( "%s: line %d\n", path, linenum );
		match++;
		if ( !displayall ) {
		    goto closetran;
		}
	    } 
	}
closetran:
	if ( fclose( f ) != 0 ) {
	    perror( path );
	    exit( 2 );
	}
	if ( !displayall && match ) {
	    goto done;
	}
    }

done:
    free_list( &head );
    acav_free( acav );

    if ( match ) {
	exit( 0 );
    } else {
	exit( 1 );
    }
}
