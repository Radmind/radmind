/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

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
#include "progress.h"
#include "report.h"

void		(*logger)( char * ) = NULL;
int		linenum = 0;
int		cksum = 0;
int		quiet = 0;
int		verbose = 0;
int		dodots = 0;
int		special = 0;
int		network = 1;
int		change = 0;
int		case_sensitive = 1;
int		report = 1;
int		create_prefix = 0;
char		transcript[ 2 * MAXPATHLEN ] = { 0 };
char		prepath[ MAXPATHLEN ]  = { 0 };

extern char	*version, *checksumlist;
extern off_t	lsize;
extern int	showprogress;
const EVP_MD    *md;
SSL_CTX  	*ctx;

extern char             *caFile, *caDir, *cert, *privatekey;

struct node {
    char                *path;
    int			doline;
    char		tline[ MAXPATHLEN * 2 ];
    struct node         *next;
};

struct node* create_node( char *path, char *tline );
void free_node( struct node *node );
int do_line( char *tline, int present, struct radstat *rs, SNET *sn );

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

    int
do_line( char *tline, int present, struct radstat *rs, SNET *sn )
{
    char        	        *command = "", *d_path;
    ACAV               		*acav;
    int				tac;
    char 	               	**targv;
    char     	       		path[ 2 * MAXPATHLEN ];
    char			temppath[ 2 * MAXPATHLEN ];
    char			pathdesc[ 2 * MAXPATHLEN ];
    char			cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    char			*xpath = NULL, *xname = NULL;
    char			*p;

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
	switch ( *targv[ 0 ] ) {
	case 'a':
	case 'f':
#ifdef ENABLE_XATTR
	case 'e':
#endif /* ENABLE_XATTR */
	    break;

	default:
	    fprintf( stderr, "line %d: \"%c\" invalid download type\n",
		    linenum, *targv[ 0 ] );
	    return( 1 );
	}

	strcpy( cksum_b64, targv[ tac - 1 ] );

	if ( special ) {
	    if ( snprintf( pathdesc, MAXPATHLEN * 2, "SPECIAL %s",
		    targv[ 1 ]) >= ( MAXPATHLEN * 2 )) {
		fprintf( stderr, "SPECIAL %s: too long\n", targv[ 1 ]);
		return( 1 );
	    }
	} else {
#ifdef ENABLE_XATTR
	    if ( *targv[ 0 ] == 'e' ) {
		if (( xpath = xattr_get_path( targv[ 1 ] )) == NULL ) {
		    fprintf( stderr, "%s: bad xattr path\n", targv[ 1 ] );
		    return( 1 );
		}
		if (( xname = xattr_get_name( targv[ 1 ], NULL )) == NULL ) {
		    fprintf( stderr, "%s: bad xattr name\n", targv[ 1 ] );
		    return( 1 );
		}
	    }
#endif /* ENABLE_XATTR */
	    if ( snprintf( pathdesc, MAXPATHLEN * 2, "%s %s %s",
		    *targv[ 0 ] == 'e' ? "XATTR" : "FILE",
		    transcript, targv[ 1 ]) >= ( MAXPATHLEN * 2 )) {
		fprintf( stderr, "FILE %s %s: command too long\n",
			transcript, targv[ 1 ]);
		return( 1 );
	    }
	}
	if ( *targv[ 0 ] == 'a' ) {
	    switch ( retr_applefile( sn, pathdesc, path, temppath, 0600,
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
	} else if ( *targv[ 0 ] == 'f' ) {
	    switch ( retr( sn, pathdesc, path, temppath, 0600,
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
	}
#ifdef ENABLE_XATTR
	  else if ( *targv[ 0 ] == 'e' ) {
	    if (( xname = xattr_name_decode( xname )) == NULL ) {
		return( 1 );
	    }

	    switch ( retr_xattr( sn, pathdesc, xpath, xname,
			strtoofft( targv[ 2 ], NULL, 10 ), cksum_b64 )) {
	    case -1:
		/* network problem */
		network = 0;
	    case 1 :
		return( 1 );
	    default:
		if ( !quiet && !showprogress ) {
		    printf( "%s: %s extended attributed applied\n",
				xpath, xname );
		}
		break;
	    }
	}
#endif /* ENABLE_XATTR */

	if ( *targv[ 0 ] != 'e' ) {
	    if ( radstat( temppath, rs ) < 0 ) {
		perror( temppath );
		return( 1 );
	    }
	    /* Update temp file*/
	    switch( update( temppath, path, present, 1, rs, tac, targv )) {
	    case 0:
		/* rename doesn't mangle forked files */
		if ( rename( temppath, path ) != 0 ) {
		    perror( temppath );
		    return( 1 );
		}
		break;

	    case 2:
		break;

	    default:
		return( 1 );
	    }
	}
    } else { 
	/* UPDATE */
	if ( present ) {
	    if ( radstat( path, rs ) < 0 ) {
		perror( path );
		return( 1 );
	    }
	}
	switch ( update( path, path, present, 0, rs, tac, targv )) {
        case 0:
        case 2:	    /* door or socket, can't be created, but not an error */
            break;
        default:
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
    int			c, err = 0;
    unsigned short	port = 0;
    extern int          optind;
    FILE		*f = NULL; 
    char		*host = _RADMIND_HOST, *d_path;
    char		tline[ 2 * MAXPATHLEN ];
    char		targvline[ 2 * MAXPATHLEN ];
    char		path[ 2 * MAXPATHLEN ];
    struct radstat	rs;
    int			tac, present = 0, len;
    char		**targv;
    char		*command = "";
    struct node		*head = NULL, *new_node, *node;
    ACAV		*acav;
    SNET		*sn = NULL;
    int			authlevel = _RADMIND_AUTHLEVEL;
    int			force = 0;
    int			use_randfile = 0;
    char	        **capa = NULL;		/* capabilities */
    char		* event = "lapply";	/* report event type */

#ifdef ENABLE_XATTR
    char		*xpath = NULL, *xname = NULL;
#endif /* ENABLE_XATTR */

    while (( c = getopt( argc, argv,
	    "%c:Ce:Fh:iInp:P:qru:Vvw:x:y:z:Z:" )) != EOF ) {
	switch( c ) {
	case '%':
	    showprogress = 1;
	    break;

	case 'c':
            OpenSSL_add_all_digests();
            md = EVP_get_digestbyname( optarg );
            if ( !md ) {
                fprintf( stderr, "%s: unsupported checksum\n", optarg );
                exit( 2 );
            }
            cksum = 1;
            break;

	case 'C':
	    create_prefix = 1;
	    break;

	case 'e':		/* set the event label for reporting */
	    event = optarg;
	    break;

	case 'F':
	    force = 1;
	    break;

	case 'h':
	    host = optarg;
	    break;

	case 'i':
	    setvbuf( stdout, ( char * )NULL, _IOLBF, 0 );
	    break;

	case 'I':
	    case_sensitive = 0;
	    break;
	
	case 'n':
	    network = 0;
	    break;

	case 'p':
	    /* connect.c handles things if atoi returns 0 */
	    port = htons( atoi( optarg ));
	    break;

        case 'P' :              /* ca dir */
            caDir = optarg;
            break;

	case 'q':
	    quiet = 1;
	    break;

	case 'r':
	    use_randfile = 1;
	    break;

        case 'u' :              /* umask */
            umask( (mode_t)strtol( optarg, (char **)NULL, 0 ));
            break;

	case 'V':
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
	    exit( 0 );

	case 'v':
	    verbose = 1;
	    logger = v_logger;
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
            caFile = optarg;
            break;

        case 'y' :              /* cert file */
            cert = optarg;
            break;

        case 'z' :              /* private key */
            privatekey = optarg;
            break;

        case 'Z':
#ifdef HAVE_ZLIB
            zlib_level = atoi(optarg);
            if (( zlib_level < 0 ) || ( zlib_level > 9 )) {
                fprintf( stderr, "Invalid compression level\n" );
                exit( 1 );
            }
            break;
#else /* HAVE_ZLIB */
            fprintf( stderr, "Zlib not supported.\n" );
            exit( 1 );
#endif /* HAVE_ZLIB */

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
	showprogress = 0;
	f = stdin; 
    } else if ( argc - optind == 1 ) {
	if (( f = fopen( argv[ optind ], "r" )) == NULL ) { 
	    perror( argv[ optind ]);
	    exit( 2 );
	}
	if ( showprogress ) {
	    lsize = applyloadsetsize( f );
	}
    } else {
	err++;
    }

    if ( quiet && ( verbose || showprogress )) {
	err++;
    }
    if ( verbose && showprogress ) {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "usage: %s [ -CFiInrV ] [ -%% | -q | -v ] ",
	    argv[ 0 ] );
	fprintf( stderr, "[ -c checksum ] [ -h host ] [ -p port ] " );
	fprintf( stderr, "[ -P ca-pem-directory ] [ -u umask ] " );
	fprintf( stderr, "[ -w auth-level ] [ -x ca-pem-file ] " );
	fprintf( stderr, "[ -y cert-pem-file ] [ -z key-pem-file ] " );
	fprintf( stderr, "[ -Z compression-level ] " );
	fprintf( stderr, "[ appliable-transcript ]\n" );
	exit( 2 );
    }

    if ( !network ) {
	authlevel = 0;
    }

    if ( authlevel != 0 ) {
        if ( tls_client_setup( use_randfile, authlevel, caFile, caDir, cert, 
                privatekey ) != 0 ) {
            /* error message printed in tls_setup */
            exit( 2 );
        }
    }

    if ( network ) {
	if (( sn = connectsn( host, port )) == NULL ) {
	    exit( 2 );
	}
	if (( capa = get_capabilities( sn )) == NULL ) {
		exit( 2 );
	}

	if ( authlevel != 0 ) {
	    if ( tls_client_start( sn, host, authlevel ) != 0 ) {
		/* error message printed in tls_cleint_starttls */
		exit( 2 );
	    }
        }

#ifdef HAVE_ZLIB
	/* Enable compression */
	if ( zlib_level > 0 ) {
	    if ( negotiate_compression( sn, capa ) != 0 ) {
		    exit( 2 );
	    }
	}
#endif /* HAVE_ZLIB */

	/* Turn off reporting if server doesn't support it */
	if ( check_capability( "REPO", capa ) == 0 ) {
	    report = 0;
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
	if ( strlen( tline ) >= MAXPATHLEN * 2 ) {
	    fprintf( stderr, "line %d: too long\n", linenum );
	    goto error2;
	}
	strcpy( targvline, tline );

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
	if ( *prepath != '\0' ) {
	    if ( pathcasecmp( path, prepath, case_sensitive ) < 0 ) {
		fprintf( stderr, "line %d: bad sort order\n", linenum );
		goto error2;
	    }
	}
	if ( strlen( path ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s: line %d: path too long\n",
		    transcript, linenum );
	    goto error2;
	}
	strcpy( prepath, path );

	if ( *targv[ 0 ] == 'e' ) {
#ifdef ENABLE_XATTR
	    if (( xpath = xattr_get_path( path )) == NULL ) {
		fprintf( stderr, "line %d: bad xattr path\n", linenum );
		goto error2;
	    }
	    if (( xname = xattr_get_name( path, NULL )) == NULL ) {
		fprintf( stderr, "line %d: bad xattr name\n", linenum );
		goto error2;
	    }
	    if (( xname = xattr_name_decode( xname )) == NULL ) {
		fprintf( stderr, "line %d: %s\n", linenum, strerror( errno ));
		goto error2;
	    }

	    memset( &rs, 0, sizeof( struct radstat ));
	    rs.rs_type = 'e';

#else /* ENABLE_XATTR */
	    fprintf( stderr, "line %d: no xattr support\n", linenum );
	    goto error2;
#endif /* ENABLE_XATTR */
	} else {
	    /* Do type check on local file */
	    switch ( radstat( path, &rs )) {
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

	    if ( present && force && ( rs.rs_stat.st_flags & CHFLAGS )) {
		if ( chflags( path, rs.rs_stat.st_flags & ~CHFLAGS ) < 0 ) {
		    perror( path );
		    goto error2;
		}
	    }
#endif /* UF_IMMUTABLE */
	}

	if ( *command == '-'
		|| ( present && rs.rs_type != *targv[ 0 ] )) {
	    if ( rs.rs_type == 'd' ) {
dirchecklist:
		if ( head == NULL ) {
		    /* Add dir to empty list */
		    if ( present && rs.rs_type != *targv[ 0 ] ) {
		    	head = create_node( path, tline );
		    } else {
			head = create_node( path, NULL);
		    }
		    continue;
		} else {
		    if ( ischildcase( path, head->path, case_sensitive )) {
			/* Add dir to list */
			if ( present && rs.rs_type != *targv[ 0 ] ) {
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
			if ( !quiet && !showprogress ) {
			    printf( "%s: deleted\n", head->path );
			}
			if ( showprogress ) {
			    progressupdate( PROGRESSUNIT, head->path );
			}
			node = head;
			head = node->next;
			if ( node->doline ) {
			    if ( do_line( node->tline, 0, &rs, sn ) != 0 ) {
				goto error2;
			    }
			    change = 1;
			}
			free_node( node );
			goto dirchecklist;
		    }
		}
#ifdef ENABLE_XATTR
	    } else if ( rs.rs_type == 'e' ) {
		/* we don't need to worry about depth-first removals yet. */
		errno = 0;
		if ( xattr_remove( xpath, xname ) != 0 ) {
		    /*
		     * We ignore ENOENT here because we're presuming that
		     * it happens when the parent file of the xattr
		     * was removed first. To do this properly, we'd need
		     * to rewrite the logic of this deletion loop to handle
		     * depth-first deletion of xattrs. That would involve
		     * significant changes, such as allowing the head node
		     * to be a file and all that entails. For this pass,
		     * just pretend ENOENT means the parent was deleted by
		     * lapply before we could delete the xattr.
		     */
		    if ( errno != ENOENT ) {
			perror( path );
			goto error2;
		    }
		    /* XXX xattr silently move on? warn? */
		}
		if ( !quiet && !showprogress ) {
		    printf( "%s: xattr deleted\n", path );
		}
		if ( showprogress ) {
		    progressupdate( PROGRESSUNIT, path );
		}
#endif /* ENABLE_XATTR */
	    } else {
filechecklist:
		if ( head == NULL ) {
		    if ( unlink( path ) != 0 ) {
			perror( path );
			goto error2;
		    }
		    if ( !quiet && !showprogress ) {
			printf( "%s: deleted\n", path );
		    }
		    if ( showprogress ) {
			progressupdate( PROGRESSUNIT, path );
		    }
		} else {
		    if ( ischildcase( path, head->path, case_sensitive )) {
			if ( unlink( path ) != 0 ) {
			    perror( path );
			    goto error2;
			}
			if ( !quiet && !showprogress ) {
			    printf( "%s: deleted\n", path );
			}
			if ( showprogress ) {
			    progressupdate( PROGRESSUNIT, path );
			}
		    } else {
			/* remove head */
			if ( rmdir( head->path ) != 0 ) {
			    perror( head->path );
			    goto error2;
			}
			if ( !quiet && !showprogress ) {
			    printf( "%s: deleted\n", head->path );
			}
			if ( showprogress ) {
			    progressupdate( PROGRESSUNIT, head->path );
			}
			node = head;
			head = node->next;
			if ( node->doline ) {
			    if ( do_line( node->tline, 0, &rs, sn ) != 0 ) {
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
	while ( head != NULL && !ischildcase( path, head->path,
		case_sensitive )) {
	    /* remove head */
	    if ( rmdir( head->path ) != 0 ) {
		perror( head->path );
		goto error2;
	    }
	    if ( !quiet && !showprogress ){
		printf( "%s: deleted\n", head->path );
	    }
	    if ( showprogress ) {
		progressupdate( PROGRESSUNIT, head->path );
	    }
	    node = head;
	    head = node->next;
	    if ( node->doline ) {
		if ( do_line( node->tline, 0, &rs, sn ) != 0 ) {
		    goto error2;
		}
		change = 1;
	    }
	    free_node( node );
	}

	if ( do_line( tline, present, &rs, sn ) != 0 ) {
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
	if ( !quiet && !showprogress ) printf( "%s: deleted\n", head->path );
	if ( showprogress ) {
	    progressupdate( PROGRESSUNIT, head->path );
	}
	node = head;
	head = node->next;
	if ( node->doline ) {
	    if ( do_line( node->tline, 0, &rs, sn ) != 0 ) {
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
	if ( report ) {
	    if ( report_event( sn, event,
		    "Changes applied successfully" ) != 0 ) {
		fprintf( stderr, "warning: could not report event\n" );
	    }
	}
	if (( closesn( sn )) != 0 ) {
	    fprintf( stderr, "cannot close sn\n" );
	    exit( 2 );
	}
#ifdef HAVE_ZLIB
	if ( verbose && zlib_level > 0 ) print_stats( sn );
#endif /* HAVE_ZLIB */
    }

    exit( 0 );

error2:
    fclose( f );
error1:
    if ( network ) {
#ifdef HAVE_ZLIB
	if( verbose && zlib_level < 0 ) print_stats(sn);
#endif /* HAVE_ZLIB */
	if ( change ) {
	    if ( network && report ) {
		if ( report_event( sn, event, "Error, changes made" ) != 0 ) {
		    fprintf( stderr, "warning: could not report event\n" );
		}
	    }
	} else {
	    if ( network && report ) {
		if ( report_event( sn, event,
			"Error, no changes made" ) != 0 ) {
		    fprintf( stderr, "warning: could not report event\n" );
		}
	    }
	}
	closesn( sn );
    }
    if ( change ) {
	exit( 1 );
    } else {
	exit( 2 );
    }
}
