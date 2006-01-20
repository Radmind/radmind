#include "config.h"

#include <sys/types.h>
#include <sys/param.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "argcargv.h"
#include "code.h"
#include "pathcmp.h"

int	linecount;
FILE	*outtran;

struct save_line {
    struct save_line 	*next;
    char 		*key;
    char 		*data;
} *lines;

void 			save_it( char *buffer, char *pathname );
static int 		lsort_cmp( const void *a1, const void *b1 );
void 			sort_them( void );
void 			print_them( void );
void 			process( char * arg );

int			case_sensitive = 1;

    void
save_it( char *buffer, char *pathname )
{
    struct save_line 	*sp;

    sp = malloc( sizeof( *sp ) + strlen( buffer ) + strlen( pathname ) + 4 );
    sp->key = (char*)( sp + 1 );
    strcpy( sp->key, pathname );
    sp->data = ( sp->key + strlen( sp->key ) + 1 );
    strcpy( sp->data, buffer );
    sp->next = lines;
    lines = sp;
    linecount++;
}

    static int
lsort_cmp( const void *a1, const void *b1 )
{
    const struct	save_line **a, **b;

    a = (const struct save_line**)a1;
    b = (const struct save_line**)b1;

    return( pathcasecmp((*a)->key, (*b)->key, case_sensitive ));
}

    void
sort_them( void )
{
    struct save_line	**x, *sp, **y;

    x = (struct save_line**) malloc( sizeof( *x ) * linecount );
    y = x;

    for ( sp = lines; sp; sp = sp->next ) {
	*y++ = sp;
    }
    
    qsort( x, linecount, sizeof *x, lsort_cmp );

    sp = 0;
    while ( y-- != x ) {
	(*y)->next = sp;
	sp = (*y);
    }

    lines = sp;
}

    void
print_them( void )
{
    struct save_line *sp;
    for ( sp = lines; sp; sp = sp->next ) {
	fputs( sp->data, outtran );
	if ( ferror( outtran )) {
	    perror( "fputs" );
	    exit( 2 );
	}
    }
}

    void
process( char *arg )
{
    FILE	*f;
    ACAV	*acav;
    char	buffer[4096];
    char	*fn;
    int		lineno, argc;
    char	**argv;
    char	*line = NULL;

    if ( strcmp( arg, "-" )) {
	fn = arg;
	f = fopen( arg, "r" );
    } else {
	fn = "(stdin)";
	f = stdin;
    }
    if ( !f ) {
	    perror( arg );
	    exit( 2 );
    }

    acav = acav_alloc();

    lineno = 0;
    while ( fgets( buffer, sizeof buffer, f )) {
	lineno++;

	if (( line = strdup( buffer )) == NULL ) {
	    perror( "strdup" );
	    exit( 1 );
	}

	argc = acav_parse( acav, buffer, &argv );

	/* Skip blank lines */
	if ( argc == 0 ) {
	    continue;
	}

	/* XXX - Drop comments - how would you sort them? */
	if ( *argv[ 0 ] == '#' ) {
	    continue;
	}

	/* Get argument offset */
	if (( *argv[ 0 ] ==  '+' ) || ( *argv[ 0 ] == '-' )) {
	    argv++;
	    argc--;
	}

	if ( argc < 2 ) {
	    fprintf( stderr, "%s: line %d: not enough fields\n", fn, lineno );
	    exit( 1 );
	}
	save_it( line, decode( argv[ 1 ] ));
    }

    if ( f == stdin ) {
	clearerr( f );
    } else {
	fclose( f );
    }

    free( line );
    acav_free( acav );
}

    int
main( int argc, char **argv )
{
    char	c;
    int		i, err = 0;
    extern char	*version;

    outtran = stdout;

    while (( c = getopt( argc, argv, "Io:V" )) != EOF ) {
	switch( c ) {
	case 'I':
	    case_sensitive = 0;
	    break;

	case 'o':
	    if (( outtran = fopen( optarg, "w" )) == NULL ) {
		perror( optarg );
		exit( 1 );
	    }
	    break;

	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );

	default:
	    err++;
	    break;
	}
    }

    if ( err ) {
	fprintf( stderr, "Usage: %s [ -IV ] ", argv[ 0 ] );
	fprintf( stderr, "[ -o file ] [ files... ]\n" );
	exit( 1 );
    }

    if ( argc - optind == 0 ) {
	/* Only stdin */
	process( "-" );
    } else {
	/* Process all args */
	for ( i = optind; i < argc; i++ ) {
	    if ( strcmp( argv[ i ], "-" ) == 0 ) {
		process( "-" );
	    } else {
		process( argv[ i ] );
	    }
	}
    }

    sort_them();
    print_them();

    exit( 0 );
}
