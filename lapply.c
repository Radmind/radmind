#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <netdb.h>
#include <snet.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "connect.h"
#include "argcargv.h"
#include "retr.h"
#include "convert.h"
#include "code.h"
#include "pathcmp.h"
#include "update.h"

int apply( FILE *f, char *parent, SNET *sn );
void output( char *string);

void		(*logger)( char * ) = NULL;
int		linenum = 0;
int		chksum = 0;
int		quiet = 0;
int		verbose = 0;
int		special = 0;
int		network = 1;
char		transcript[ 2 * MAXPATHLEN ] = { 0 };
char		prepath[ MAXPATHLEN ]  = { 0 };
extern char	*version, *checksumlist;

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

    static int
ischild( const char *child, const char *parent )
{
    size_t parentlen;

    if ( parent == NULL ) {
	return 1;
    } else {
	parentlen = strlen( parent );
	if( parentlen > strlen( child )) {
	    return 0;
	}
	if(( strncmp( parent, child, parentlen ) == 0 ) &&
		child[ parentlen ] == '/' ) {
	    return 1;
	} else {
	    return 0;
	}
    }
}

    void
output( char *string )
{
    printf( "<<< %s\n", string );
    return;
}

/*
 * Never exit.  Must return so main can close network connection.
 */

    int 
apply( FILE *f, char *parent, SNET *sn )
{
    char		tline[ 2 * MAXPATHLEN ];
    char		path[ 2 * MAXPATHLEN ];
    char		temppath[ 2 * MAXPATHLEN ];
    char		pathdesc[ 2 * MAXPATHLEN ];
    char		chksum_b64[ 29 ];
    int			tac, present, len;
    char		**targv;
    char		*command = "";
    char		fstype;
    struct stat		st;
    struct node		*head= NULL, *new_node, *node;
    ACAV		*acav;

    acav = acav_alloc( );

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	len = strlen( tline );
        if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line %d: line too long\n", tline, linenum  );
	    return( 1 );
	}

	tac = acav_parse( acav, tline, &targv );

	if ( tac == 1 ) {
	    strcpy( transcript, targv[ 0 ] );
	    len = strlen( transcript );
	    if ( transcript[ len - 1 ] != ':' ) { 
		fprintf( stderr, "%s: line %d: invalid transcript name\n",
		    transcript, linenum );
		return( 1 );
	    }
	    transcript[ len - 1 ] = '\0';
	    if ( strcmp( transcript, "special.T" ) == 0 ) {
		special = 1;
	    } else {
		special = 0;
	    }
	    if ( verbose ) printf( "Transcript: %s\n", transcript );
	    continue;
	}

	/* Get argument offset */
	if (( *targv[ 0 ] ==  '+' )
		|| ( *targv[ 0 ] == '-' )) {
	    command = targv[ 0 ];
	    targv++;
	    tac--;
	}

	if (( *command == '+' ) && ( !network )) {
	    continue;
	}

	strcpy( path, decode( targv[ 1 ] ));

	/* Check transcript order */
	if ( prepath != 0 ) {
	    if ( pathcmp( path, prepath ) < 0 ) {
		fprintf( stderr, "%s: line %d: bad sort order\n",
			    transcript, linenum );
		return( 1 );
	    }
	}
	len = strlen( path );
	if ( snprintf( prepath, MAXPATHLEN, "%s", path) > MAXPATHLEN ) { 
	    fprintf( stderr, "%s: line %d: path too long\n",
		    transcript, linenum );
	    return( 1 );
	}

	/* Do type check on local file */
	if ( lstat( path, &st ) == 0 ) {
	    fstype = t_convert( path, NULL, (int)( S_IFMT & st.st_mode ));
	    present = 1;
	} else if ( errno == ENOENT ) { 
	    present = 0;
	} else {
	    perror( path );
	    return( 1 );
	}

	if ( *command == '-'
		|| ( present && fstype != *targv[ 0 ] )) {
	    if ( fstype == 'd' ) {
dirchecklist:
		if ( head == NULL ) {
		    /* Add dir to empty list */
		    head = create_node( path );
		    continue;
		} else {
		    if ( ischild( path, head->path)) {
			/* Add dir to list */
			new_node = create_node( path );
			new_node->next = head;
			head = new_node;
		    } else {
			/* remove head */
			if ( rmdir( head->path ) != 0 ) {
			     perror( head->path );
			     return( 1 );
			}
			if ( !quiet ) printf( "%s: deleted\n", path );
			node = head;
			head = node->next;
			free_node( node );
			goto dirchecklist;
		    }
		}
	    } else {
filechecklist:
		if ( head == NULL ) {
		    if ( unlink( path ) != 0 ) {
			perror( path );
			return( 1 );
		    }
		    if ( !quiet ) printf( "%s: deleted\n", path );
		} else {
		    if ( ischild( path, head->path)) {
			if ( unlink( path ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( !quiet ) printf( "%s: deleted\n", path );
		    } else {
			/* remove head */
			if ( rmdir( head->path ) != 0 ) {
			     perror( head->path );
			     return( 1 );
			}
			if ( !quiet ) printf( "%s: deleted\n", path );
			node = head;
			head = node->next;
			free_node( node );
			goto filechecklist;
		    }
		}
	    }
	    present = 0;

	    if ( *command == '-' ) {
		continue;
	    }
	}

	/* DOWNLOAD */
	if ( *command == '+' ) {
#ifdef __APPLE__
	    if (( *targv[ 0 ] != 'f' ) || ( *targv[ 0 ] != 'a' )) {
#else !__APPLE__
	    if ( *targv[ 0 ] != 'f' ) {
#endif __APPLE__
		fprintf( stderr, "line %d: \"%c\" invalid download type\n",
			linenum, *targv[ 0 ] );
		return( 1 );
	    }

	    strcpy( chksum_b64, targv[ 7 ] );

	    if ( special ) {
		if ( snprintf( pathdesc, MAXPATHLEN * 2, "SPECIAL %s",
			encode( path ))) {
		    fprintf( stderr, "SPECIAL %s: too long\n", encode( path ));
		    return( 1 );
		}
	    } else {
		if ( snprintf( pathdesc, MAXPATHLEN * 2, "FILE %s %s",
			transcript, encode( path )) > ( MAXPATHLEN * 2 ) -1 ) {
		    fprintf( stderr, "FILE %s %s: command too long\n",
			transcript, encode( path ));
		    return( 1 );
		}
	    }

	    if ( retr( sn, pathdesc, path, NULL, chksum_b64,
		    (char *)&temppath ) != 0 ) {
		return( 1 );
	    }

	    /* DO LSTAT ON NEW FILE */
	    if ( lstat( temppath, &st ) !=  0 ) {
		perror( temppath );
		return( 1 );
	    }
	    fstype = t_convert( path, NULL, (int)( S_IFMT & st.st_mode ));

	    /* Update temp file*/
	    if ( update( temppath, path, present, 1, st, tac, targv )
		    != 0 ) {
		perror( "update" );
		return( 1 );
	    }
#ifdef __APPLE__
	    // Convert apple single -> apple file here
#endif __APPLE__
	    if ( rename( temppath, path ) != 0 ) {
		perror( temppath );
		return( 1 );
	    }

	} else { 
	    /* UPDATE */
	    if ( update( path, path, present, 0, st, tac, targv ) != 0 ) {
		perror( "update" );
		return( 1 );
	    }
	}

    }
    /* Clear out dir list */ 
    while ( head != NULL ) {
	/* remove head */
	if ( rmdir( head->path ) != 0 ) {
	     perror( head->path );
	     return( 1 );
	}
	if ( quiet ) printf( "%s: deleted\n", path );
	node = head;
	head = node->next;
	free_node( node );
    }

    acav_free( acav ); 
    return( 0 );
}

    int
main( int argc, char **argv )
{
    int			c, port = htons( 6662 ), err = 0;
    extern int          optind;
    FILE		*f; 
    char		*host = _RADMIND_HOST;
    struct servent	*se;
    SNET		*sn;

    while (( c = getopt ( argc, argv, "c:h:np:qVv" )) != EOF ) {
	switch( c ) {
	case 'c':
	    if ( strcasecmp( optarg, "sha1" ) != 0 ) {
		perror( optarg );
		exit( 1 );
	    }
	    chksum = 1;
	    break;
	case 'h':
	    host = optarg;
	    break;
	case 'n':
	    network = 0;
	    break;
	case 'p':
	    if (( port = htons ( atoi( optarg ))) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 1 );
		}
		port = se->s_port;
	    }
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'V':
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
	    exit( 0 );
	case 'v':
	    verbose = 1;
	    logger = output;
	    break;
	case '?':
	    err++;
	    break;
	default:
	    err++;
	    break;
	}
    }

    if (( host == NULL ) &&  network ) {
	err++;
    }

    if ( argc - optind == 0 ) {
	f = stdin; 
    } else if ( argc - optind == 1 ) {
	if (( f = fopen( argv[ optind ], "r" )) == NULL ) { 
	    perror( optind );
	    goto error0;
	}
    } else {
	err++;
    }
    if ( verbose && quiet ) {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "usage: lapply [ -nsV ] [ -q | -v ]" );
	fprintf( stderr, "[ -c checksum ] [ -h host ] [ -p port ] " );
	fprintf( stderr, "[ appliable-transcript ]\n" );
	exit( 1 );
    }

    if ( network ) {
	if(( sn = connectsn( host, port )  ) == NULL ) {
	    fprintf( stderr, "%s:%d connection failed.\n", host, port );
	    goto error0;
	}
    } else {
	if ( !quiet ) printf( "No network connection\n" );
    }

    if ( apply( f, NULL, sn ) != 0 ) {
	goto error2;
    }
    
    if ( fclose( f ) != 0 ) {
	perror( argv[ optind ] );
	goto error1;
    }

    if ( network ) {
	if (( closesn( sn )) !=0 ) {
	    fprintf( stderr, "can not close sn\n" );
	    goto error0;
	}
    }

    exit( 0 );

error2:
    if ( fclose( f ) != 0 ) {
	perror( argv[ optind ] );
	exit( 1 );
    }

error1:
    if ( network ) {
	if (( closesn( sn )) !=0 ) {
	    fprintf( stderr, "can not close sn\n" );
	    exit( 1 );
	}
    }

error0:
    exit( 1 );
}
