/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <openssl/evp.h>
#include <snet.h>

#include "applefile.h"
#include "base64.h"
#include "cksum.h"
#include "connect.h"
#include "argcargv.h"
#include "radstat.h"
#include "code.h"
#include "pathcmp.h"
#include "update.h"
#include "tls.h"
#include "largefile.h"

void		(*logger)( char * ) = NULL;
int		linenum = 0;
int		cksum = 0;
int		quiet = 0;
int		verbose = 0;
int		dodots = 0;
int		special = 0;
int		network = 1;
int		change = 0;
char		transcript[ 2 * MAXPATHLEN ] = { 0 };
char		prepath[ MAXPATHLEN ]  = { 0 };
extern char	*version, *checksumlist;
const EVP_MD    *md;
SSL_CTX  	*ctx;

extern char             *ca, *cert, *privatekey;

struct node {
    char                *path;
    int			doline;
    char		tline[ MAXPATHLEN * 2 ];
    struct node         *next;
};

struct node* create_node( char *path, char *tline );
void free_node( struct node *node );
int do_line( char *tline, int present, struct stat *st, SNET *sn );
void output( char *string);

   struct node *
create_node( char *path, char *tline )
{
    struct node         *new_node;

    new_node = (struct node *) malloc( sizeof( struct node ));
    new_node->path = strdup( path );
    if ( tline != NULL ) {
	sprintf( new_node->tline, "%s", tline );
	new_node->doline = 1;
    } else {
	new_node->doline = 0;
    }
    new_node->next = NULL;

    return( new_node );
}

    void 
free_node( struct node *node )
{
    free( node->path );
    free( node );
}

    void
output( char *string )
{
    printf( "<<< %s\n", string );
    return;
}

    int
do_line( char *tline, int present, struct stat *st, SNET *sn )
{
    char                	fstype;
    char        	        *command = "", *d_path;
    ACAV               		*acav;
    int				tac;
    char 	               	**targv;
    struct applefileinfo        afinfo;
    char     	       		path[ 2 * MAXPATHLEN ];
    char			temppath[ 2 * MAXPATHLEN ];
    char			pathdesc[ 2 * MAXPATHLEN ];
    char			cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    acav = acav_alloc( );

    tac = acav_parse( acav, tline, &targv );
    /* Get argument offset */
    if (( *targv[ 0 ] ==  '+' ) || ( *targv[ 0 ] == '-' )) {
	command = targv[ 0 ];
	targv++;
	tac--;
    }
    if (( d_path = decode( targv[ 1 ] )) == NULL ) {
	fprintf( stderr, "line %d: too long\n", linenum );
	return( 1 );
    } 
    strcpy( path, d_path );

    /* DOWNLOAD */
    if ( *command == '+' ) {
	if (( *targv[ 0 ] != 'f' ) && ( *targv[ 0 ] != 'a' )) {
	    fprintf( stderr, "line %d: \"%c\" invalid download type\n",
		    linenum, *targv[ 0 ] );
	    return( 1 );
	}
	strcpy( cksum_b64, targv[ 7 ] );

	if ( special ) {
	    if ( snprintf( pathdesc, MAXPATHLEN * 2, "SPECIAL %s",
		    targv[ 1 ]) > ( MAXPATHLEN * 2 ) - 1 ) {
		fprintf( stderr, "SPECIAL %s: too long\n", targv[ 1 ]);
		return( 1 );
	    }
	} else {
	    if ( snprintf( pathdesc, MAXPATHLEN * 2, "FILE %s %s",
		    transcript, targv[ 1 ]) > ( MAXPATHLEN * 2 ) -1 ) {
		fprintf( stderr, "FILE %s %s: command too long\n",
		    transcript, targv[ 1 ]);
		return( 1 );
	    }
	}
	if ( *targv[ 0 ] == 'a' ) {
	    switch ( retr_applefile( sn, pathdesc, path, temppath,
		strtoofft( targv[ 6 ], NULL, 10 ), cksum_b64 )) {
	    case -1:
		/* Network problem */
		network = 0;
		return( 1 );
	    case 1:
		return( 1 );
	    default:
		break;
	    }
	} else {
	    switch ( retr( sn, pathdesc, path, (char *)&temppath,
		strtoofft( targv[ 6 ], NULL, 10 ), cksum_b64 ) != 0 ) {
	    case -1:
		/* Network problem */
		network = 0;
		return( 1 );
	    case 1:
		return( 1 );
	    default:
		break;
	    }
	}
	if ( radstat( temppath, st, &fstype, &afinfo ) < 0 ) {
	    perror( temppath );
	    return( 1 );
	}
	/* Update temp file*/
	if ( update( temppath, path, present, 1, st, tac, targv, &afinfo )
		!= 0 ) {
	    return( 1 );
	}

	 /*
	  * rename doesn't mangle forked files
	  */
	if ( rename( temppath, path ) != 0 ) {
	    perror( temppath );
	    return( 1 );
	}
    } else { 
	/* UPDATE */
	if ( present ) {
	    if ( radstat( path, st, &fstype, &afinfo ) < 0 ) {
		perror( path );
		return( 1 );
	    }
	}
	if ( update( path, path, present, 0, st, tac, targv, &afinfo ) != 0 ) {
	    return( 1 );
	}
    }
    acav_free( acav ); 
    return( 0 );
}

/*
 * exit values
 * 0 - OKAY
 * 1 - error - system modified
 * 2 - error - no modification
 */

    int
main( int argc, char **argv )
{
    int			c, port = htons( 6662 ), err = 0;
    extern int          optind;
    FILE		*f = NULL; 
    char		*host = _RADMIND_HOST, *d_path;
    struct servent	*se;
    char		tline[ 2 * MAXPATHLEN ];
    char		targvline[ 2 * MAXPATHLEN ];
    char		path[ 2 * MAXPATHLEN ];
    struct applefileinfo	afinfo;
    int			tac, present, len, lnbf = 0;
    char		**targv;
    char		*command = "";
    char		fstype;
    struct stat		st;
    struct node		*head = NULL, *new_node, *node;
    ACAV		*acav;
    SNET		*sn = NULL;
    int			authlevel = _RADMIND_AUTHLEVEL;
    int			force = 0;
    int			use_randfile = 0;

    while (( c = getopt ( argc, argv, "c:Fh:inp:qrVvw:x:y:z:" )) != EOF ) {
	switch( c ) {
	case 'c':
            OpenSSL_add_all_digests();
            md = EVP_get_digestbyname( optarg );
            if ( !md ) {
                fprintf( stderr, "%s: unsupported checksum\n", optarg );
                exit( 2 );
            }
            cksum = 1;
            break;

	case 'F':
	    force = 1;
	    break;

	case 'h':
	    host = optarg;
	    break;

	case 'i':
	    setvbuf( stdout, ( char * )NULL, _IOLBF, 0 );
	    lnbf = 1;
	    break;
	
	case 'n':
	    network = 0;
	    break;

	case 'p':
	    if (( port = htons ( atoi( optarg ))) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 2 );
		}
		port = se->s_port;
	    }
	    break;

	case 'q':
	    quiet = 1;
	    break;

	case 'r':
	    use_randfile = 1;
	    break;

	case 'V':
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
	    exit( 0 );

	case 'v':
	    verbose = 1;
	    logger = output;
	    if ( isatty( fileno( stdout ))) {
		dodots = 1;
	    }
	    break;

        case 'w' :              /* authlevel 0:none, 1:serv, 2:client & serv */
            authlevel = atoi( optarg );
            if (( authlevel < 0 ) || ( authlevel > 2 )) {
                fprintf( stderr, "%s: invalid authorization level\n",
                        optarg );
                exit( 1 );
            }
            break;

        case 'x' :              /* ca file */
            ca = optarg;
            break;

        case 'y' :              /* cert file */
            cert = optarg;
            break;

        case 'z' :              /* private key */
            privatekey = optarg;
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
	    perror( argv[ optind ]);
	    exit( 2 );
	}
    } else {
	err++;
    }
    if ( verbose && quiet ) {
	err++;
    }
    if ( verbose && lnbf ) {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "usage: %s [ -nrsV ] [ -q | -v | -i ] ", argv[ 0 ] );
	fprintf( stderr, "[ -c checksum ] [ -h host ] [ -p port ] " );
	fprintf( stderr, "[ -w authlevel ] [ -x ca-pem-file ] " );
	fprintf( stderr, "[ -y cert-pem-file] [ -z key-pem-file ] " );
	fprintf( stderr, "[ appliable-transcript ]\n" );
	exit( 2 );
    }

    if ( !network ) {
	authlevel = 0;
    }

    if ( authlevel != 0 ) {
        if ( tls_client_setup( use_randfile, authlevel, ca, cert, 
                privatekey ) != 0 ) {
            /* error message printed in tls_setup */
            exit( 2 );
        }
    }

    if ( network ) {
	if (( sn = connectsn( host, port )) == NULL ) {
	    exit( 2 );
	}
	if ( authlevel != 0 ) {
	    if ( tls_client_start( sn, host, authlevel ) != 0 ) {
		/* error message printed in tls_cleint_starttls */
		exit( 2 );
	    }
        }
    } else {
	if ( !quiet ) printf( "No network connection\n" );
    }

    acav = acav_alloc( );

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	/* Check line length */
	len = strlen( tline );
        if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line %d: line too long\n", tline, linenum  );
	    goto error2;
	}
	if ( snprintf( targvline, MAXPATHLEN * 2, "%s", tline )
		>= MAXPATHLEN * 2 ) {
	    fprintf( stderr, "line %d: too long\n", linenum );
	    goto error2;
	}

	tac = acav_parse( acav, targvline, &targv );

        /* Skip blank lines and comments */
        if (( tac == 0 ) || ( *targv[ 0 ] == '#' )) {
	    continue;
        }

	if ( tac == 1 ) {
	    strcpy( transcript, targv[ 0 ] );
	    len = strlen( transcript );
	    if ( transcript[ len - 1 ] != ':' ) { 
		fprintf( stderr, "%s: line %d: invalid transcript name\n",
		    transcript, linenum );
		goto error2;
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
	if (( *targv[ 0 ] ==  '+' ) || ( *targv[ 0 ] == '-' )) {

	    /* Check for transcript name on download */
	    if ( *targv[ 0 ] ==  '+' ) {
		if ( strcmp( transcript, "" ) == 0 ) {
		    fprintf( stderr, "line %d: no transcript indicated\n",
			linenum );
		    goto error2;
		}
	    }

	    command = targv[ 0 ];
	    targv++;
	    tac--;
	}

	if (( *command == '+' ) && ( !network )) {
	    continue;
	}

	if (( d_path = decode( targv[ 1 ] )) == NULL ) {
	    fprintf( stderr, "line %d: too long\n", linenum );
	    return( 1 );
	} 
	strcpy( path, d_path );

	/* Check transcript order */
	if ( prepath != 0 ) {
	    if ( pathcmp( path, prepath ) < 0 ) {
		fprintf( stderr, "%s: line %d: bad sort order\n",
			    transcript, linenum );
		goto error2;
	    }
	}
	len = strlen( path );
	if ( snprintf( prepath, MAXPATHLEN, "%s", path) > MAXPATHLEN ) { 
	    fprintf( stderr, "%s: line %d: path too long\n",
		    transcript, linenum );
	    goto error2;
	}

	/* Do type check on local file */
	switch ( radstat( path, &st, &fstype, &afinfo )) {
	case 0:
	    present = 1;
	    break;
	case 1:
	    fprintf( stderr, "%s is of an unknown type\n", path );
	    goto error2;
	default:
	    if ( errno == ENOENT ) { 
		present = 0;
	    } else {
		perror( path );
		goto error2;
	    }
	    break;
	}

#ifdef UF_IMMUTABLE
#define CHFLAGS	( UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND )

	if ( present && force && ( st.st_flags & CHFLAGS )) {
	    if ( chflags( path, st.st_flags & ~CHFLAGS ) < 0 ) {
		perror( path );
		goto error2;
	    }
	}
#endif /* UF_IMMUTABLE */

	if ( *command == '-'
		|| ( present && fstype != *targv[ 0 ] )) {
	    if ( fstype == 'd' ) {
dirchecklist:
		if ( head == NULL ) {
		    /* Add dir to empty list */
		    if ( present && fstype != *targv[ 0 ] ) {
		    	head = create_node( path, tline );
		    } else {
			head = create_node( path, NULL);
		    }
		    continue;
		} else {
		    if ( ischild( path, head->path)) {
			/* Add dir to list */
			if ( present && fstype != *targv[ 0 ] ) {
			    new_node = create_node( path, tline );
			} else {
			    new_node = create_node( path, NULL);
			}
			new_node->next = head;
			head = new_node;
		    } else {
			/* remove head */
			if ( rmdir( head->path ) != 0 ) {
			    perror( head->path );
			    goto error2;
			}
			if ( !quiet ) printf( "%s: deleted\n", head->path );
			node = head;
			head = node->next;
			if ( node->doline ) {
			    if ( do_line( node->tline, 0, &st, sn ) != 0 ) {
				goto error2;
			    }
			    change = 1;
			}
			free_node( node );
			goto dirchecklist;
		    }
		}
	    } else {
filechecklist:
		if ( head == NULL ) {
		    if ( unlink( path ) != 0 ) {
			perror( path );
			goto error2;
		    }
		    if ( !quiet ) printf( "%s: deleted\n", path );
		} else {
		    if ( ischild( path, head->path)) {
			if ( unlink( path ) != 0 ) {
			    perror( path );
			    goto error2;
			}
			if ( !quiet ) printf( "%s: deleted\n", path );
		    } else {
			/* remove head */
			if ( rmdir( head->path ) != 0 ) {
			    perror( head->path );
			    goto error2;
			}
			if ( !quiet ) printf( "%s: deleted\n", head->path );
			node = head;
			head = node->next;
			if ( node->doline ) {
			    if ( do_line( node->tline, 0, &st, sn ) != 0 ) {
				goto error2;
			    }
			    change = 1;
			}
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
	/* Minimize remove list */
	while ( head != NULL && !ischild( path, head->path )) {
	    /* remove head */
	    if ( rmdir( head->path ) != 0 ) {
		perror( head->path );
		goto error2;
	    }
	    if ( !quiet ) printf( "%s: deleted\n", head->path );
	    node = head;
	    head = node->next;
	    if ( node->doline ) {
		if ( do_line( node->tline, 0, &st, sn ) != 0 ) {
		    goto error2;
		}
		change = 1;
	    }
	    free_node( node );
	}

	if ( do_line( tline, present, &st, sn ) != 0 ) {
	    goto error2;
	}
	change = 1;
    }

    /* Clear out remove list */ 
    while ( head != NULL ) {
	/* remove head */
	if ( rmdir( head->path ) != 0 ) {
	    perror( head->path );
	    goto error2;
	}
	if ( !quiet ) printf( "%s: deleted\n", head->path );
	node = head;
	head = node->next;
	if ( node->doline ) {
	    if ( do_line( node->tline, 0, &st, sn ) != 0 ) {
		goto error2;
	    }
	    change = 1;
	}
	free_node( node );
    }
    acav_free( acav ); 
    
    if ( fclose( f ) != 0 ) {
	perror( argv[ optind ] );
	goto error1;
    }

    if ( network ) {
	if (( closesn( sn )) != 0 ) {
	    fprintf( stderr, "can not close sn\n" );
	    exit( 2 );
	}
    }

    exit( 0 );

error2:
    fclose( f );
error1:
    if ( network ) {
	closesn( sn );
    }
    if ( change ) {
	exit( 1 );
    } else {
	exit( 2 );
    }
}
