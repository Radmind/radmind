/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

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
#include "list.h"
#include "tls.h"
#include "largefile.h"
#include "mkdirs.h"
#include "rmdirs.h"

void output( char* string);
int check( SNET *sn, char *type, char *path); 
int createspecial( SNET *sn, struct list *special_list );
int getstat( SNET *sn, char *description, char *stats );
int read_kfile( char * );
SNET *sn;

void			(*logger)( char * ) = NULL;
int		linenum = 0;
int			cksum = 0;
int			verbose = 0;
int			dodots= 0;
int			quiet = 0;
int			update = 1;
int			change = 0;
char			*base_kfile= _RADMIND_COMMANDFILE;
char			*kdir= "";
const EVP_MD		*md;
SSL_CTX  		*ctx;
struct list		*special_list, *kfile_list, *kfile_seen;

extern struct timeval	timeout;
extern char		*version, *checksumlist;
extern char             *ca, *cert, *privatekey; 

    int 
getstat( SNET *sn, char *description, char *stats ) 
{
    struct timeval      tv;
    char		*line;

    if( snet_writef( sn, "STAT %s\n", description ) < 0 ) {
	perror( "snet_writef" );
	return( -1 );
    }

    if ( verbose ) printf( ">>> STAT %s\n", description );

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	perror( "snet_getline_multi" );
	return( -1 );
    }
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n",  line );
	return( -1 );
    }

    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	perror( "snet_getline 1" );
	return( -1 );
    }
    if ( strlen( line ) >= MAXPATHLEN ) {
	fprintf( stderr, "%s: line too long\n", line );
	return( -1 );
    }
    strcpy( stats, line );

    if ( verbose ) printf( "<<< %s\n", stats );

    return( 0 );
}

    void
output( char *string )
{
    printf( "<<< %s\n", string );
    return;
}

    int
createspecial( SNET *sn, struct list *special_list )
{
    FILE		*fs;
    struct node 	*node;
    char		filedesc[ MAXPATHLEN * 2 ];
    char		path[ MAXPATHLEN ];
    char		stats[ MAXPATHLEN ];

    /* Open file */
    if ( snprintf( path, MAXPATHLEN, "%sspecial.T.%i", kdir,
	    getpid()) >= MAXPATHLEN ) {
	fprintf( stderr, "path too long: %sspecial.T.%i\n", kdir,
		(int)getpid());
	exit( 2 );
    }

    if (( fs = fopen( path, "w" )) == NULL ) {
	perror( path );
	return( 1 );
    }

    for ( node = list_pop_head( special_list ); node != NULL;
	    node = list_pop_head( special_list )) {
	if ( snprintf( filedesc, MAXPATHLEN * 2, "SPECIAL %s", node->n_path)
		>= ( MAXPATHLEN * 2 )) {
	    fprintf( stderr, "SPECIAL %s: too long\n", node->n_path );
	    return( 1 );
	}

	if ( getstat( sn, (char *)&filedesc, stats ) != 0 ) {
	    return( 1 );
	}

	if ( fputs( stats, fs) == EOF ) {
	    fprintf( stderr, "fputs" );
	    return( 1 );
	}
	if ( fputs( "\n", fs) == EOF ) {
	    fprintf( stderr, "fputs" );
	    return( 1 );
	}
	free( node );
    }

    if ( fclose( fs ) != 0 ) {
	perror( path );
	return( 1 );
    }

    return( 0 );
}

/*
 * return codes:
 *	0	okay
 *	1	update made
 *	2	system error
 */

    int
check( SNET *sn, char *type, char *file )
{
    int		needupdate = 0;
    char	**targv;
    char	stats[ MAXPATHLEN ];
    char 	pathdesc[ 2 * MAXPATHLEN ];
    char 	tempfile[ 2 * MAXPATHLEN ];
    char        ccksum[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    char	path[ MAXPATHLEN ];
    char	*p;
    int		tac;
    struct stat		st;
    struct utimbuf      times;

    if ( file != NULL ) {
	if ( snprintf( pathdesc, MAXPATHLEN * 2, "%s %s", type, file  )
		>= ( MAXPATHLEN * 2 )) {
	    fprintf( stderr, "%s %s: too long", type, file );
	    return( 2 );
	}

	/* create full path */
	if ( snprintf( path, MAXPATHLEN, "%s%s", kdir, file )
		>= MAXPATHLEN ) {
	    fprintf( stderr, "%s%s: path too long\n", kdir, file );
	    return( 2 );
	}

	/* Check for transcript with directories */
	for ( p = strchr( file, '/' ); p != NULL; p = strchr( p, '/' )) {
	    *p = '\0';

	    /* Check to see if path exists as a directory */
	    if ( snprintf( tempfile, MAXPATHLEN, "%s%s", kdir, file )
		    >= MAXPATHLEN ) {
		fprintf( stderr, "%s%s: path too long\n", kdir, file );
		return( 2 );
	    }
	    if ( stat( tempfile, &st ) != 0 ) {
		if ( errno != ENOENT ) {
		    perror( tempfile );
		    return( 2 );
		} else {
		    if ( mkdir( tempfile, 0777 ) != 0 ) {
			perror( tempfile );
			return( 2 );
		    }
		}
	    } else {
		/* Make sure it is a directory */
		if ( !S_ISDIR( st.st_mode )) {
		    if ( unlink( tempfile ) != 0 ) {
			perror( tempfile );
			return( 2 );
		    }
		    if ( mkdir( tempfile, 0777 )) {
			perror( tempfile );
			return( 2 );
		    }
		}
	    }
	    *p++ = '/';
	}
	if ( stat( path, &st ) != 0 ) {
	    if ( errno != ENOENT ) {
		perror( path );
		return( 2 );
	    }
	} else {
	    if ( S_ISDIR( st.st_mode )) {
		if ( rmdirs( path ) != 0 ) {
		    perror( path );
		    return( 2 );
		}
	    }
	}


    } else {
	if ( strlen( type ) >= ( MAXPATHLEN * 2 )) {
	    fprintf( stderr, "%s: too long\n", type );
	    return( 2 );
	}
	strcpy( pathdesc, type );

	file = base_kfile;

	/* create full path */
	if ( strlen( base_kfile ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s: path too long\n", base_kfile );
	    return( 2 );
	}
	strcpy( path, base_kfile );
    }

    if ( getstat( sn, (char *)&pathdesc, stats ) != 0 ) {
	return( 2 );
    }
    tac = acav_parse( NULL, stats, &targv );
    if ( tac != 8 ) {
	perror( "Incorrect number of arguments\n" );
	return( 2 );
    }
    times.modtime = atoi( targv[ 5 ] );
    times.actime = time( NULL );

    if (( stat( path, &st )) != 0 ) {
	if ( errno != ENOENT ) {
	    perror( path );
	    return( 2 );
	} else {
	    /* Local file is missing */
	    if ( update ) {
		if ( !quiet ) { printf( "%s:", path ); fflush( stdout ); }
		if ( retr( sn, pathdesc, path, (char *)&tempfile,
			strtoofft( targv[ 6 ], NULL, 10 ), targv[ 7 ] ) != 0 ) {
		    return( 2 );
		}
		if ( utime( tempfile, &times ) != 0 ) {
		    perror( path );
		    return( 1 );
		}
		if ( rename( tempfile, path ) != 0 ) {
		    perror( tempfile );
		    return( 2 );
		}
		if ( !quiet ) printf( " updated\n" );
	    } else {
		if ( !quiet ) printf ( "%s: missing\n", path );
	    }
	    return( 1 );
	}
    }

    /*
     * With cksum we only use cksum and size.
     * Without cksum we only use mtime and size.
     */
    if ( strtoofft( targv[ 6 ], NULL, 10 ) != st.st_size ) {
	needupdate = 1;
    } else {
	if ( cksum ) {
	    if (( do_cksum( path, ccksum )) < 0 ) {
		perror( path );
		return( 2 );
	    }
	    if ( strcmp( targv[ 7 ], ccksum ) != 0 ) {
		needupdate = 1;
	    }
	} else {
	    if ( atoi( targv[ 5 ] ) != (int)st.st_mtime )  {
		needupdate = 1;
	    }
	}
    }
    if ( needupdate ) {
	if ( update ) {
	    if ( !quiet ) { printf( "%s:", path ); fflush( stdout ); }
	    if ( unlink( path ) != 0 ) {
		perror( path );
		return( 2 );
	    }
	    if ( retr( sn, pathdesc, path, (char *)&tempfile,
		    strtoofft( targv[ 6 ], NULL, 10 ), targv[ 7 ] ) != 0 ) {
		return( 2 );
	    }
	    if ( utime( tempfile, &times ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( rename( tempfile, path ) != 0 ) {
		perror( path );
		return( 2 );
	    }
	    if ( !quiet ) printf( " updated\n" );
	} else {
	    if ( !quiet ) printf( "%s: out of date\n", path );
	}
	return( 1 );
    } else {
	return( 0 );
    }
}

/*
 * exit codes:
 *      0       No changes found, everything okay
 *      1       Changes necessary / changes made
 *      2       System error
 */

    int
main( int argc, char **argv )
{
    int			c, port = htons( 6662 ), err = 0;
    int			authlevel = _RADMIND_AUTHLEVEL;
    int			use_randfile = 0;
    char	lcksum[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    char	tcksum[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    struct stat		tst, lst;
    extern int          optind;
    char		*host = _RADMIND_HOST, *p;
    char		path[ MAXPATHLEN ];
    char		tempfile[ MAXPATHLEN ];
    struct servent	*se;
    struct node		*node;

    while (( c = getopt( argc, argv, "c:h:iK:np:qrvVw:x:y:z:" )) != EOF ) {
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

	case 'h':
	    host = optarg;
	    break;

	case 'i':
	    setvbuf( stdout, ( char * )NULL, _IOLBF, 0 );
	    break;

	case 'K':
	    base_kfile = optarg;
	    break;

	case 'n':
	    update = 0;
	    break;

	case 'p':
	    if (( port = htons ( atoi( optarg )) ) == 0 ) {
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

	case 'v':
	    verbose = 1;
	    logger = output;
	    if ( isatty( fileno( stdout ))) {
		dodots = 1;
	    }
	    break;

	case 'V':
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
	    exit( 0 );

        case 'w' :              /* authlevel 0:none, 1:serv, 2:client & serv */
            authlevel = atoi( optarg );
            if (( authlevel < 0 ) || ( authlevel > 2 )) {
                fprintf( stderr, "%s: invalid authorization level\n",
                        optarg );
                exit( 2 );
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

	default:
	    err++;
	    break;
	}
    }

    if ( verbose && quiet ) {
	err++;
    }

    if ( err || ( argc - optind != 0 )) {
	fprintf( stderr, "usage: %s ", argv[ 0 ] );
	fprintf( stderr, "[ -inrV ] [ -q | -v ] " );
	fprintf( stderr, "[ -c checksum ] [ -K command file ] " );
	fprintf( stderr, "[ -h host ] [ -p port ] " );
	fprintf( stderr, "[ -w authlevel ] [ -x ca-pem-file ] " );
	fprintf( stderr, "[ -y cert-pem-file] [ -z key-pem-file ]\n" );
	exit( 2 );
    }

    if (( special_list = list_new( )) == NULL ) {
	perror( "list_new" );
	exit( 2 );
    }
    if (( kfile_list = list_new( )) == NULL ) {
	perror( "list_new" );
	exit( 2 );
    }
    if (( kfile_seen = list_new( )) == NULL ) {
	perror( "list_new" );
	exit( 2 );
    }

    if ( strlen( base_kfile ) >= MAXPATHLEN ) {
	fprintf( stderr, "%s: path too long\n", base_kfile );
	exit( 2 );
    }
    if (( kdir = strdup( base_kfile )) == NULL ) {
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
    strcpy( path, base_kfile );

    if (( sn = connectsn( host, port )) == NULL ) {
	exit( 2 );
    }

    if ( authlevel != 0 ) {
	if ( tls_client_setup( use_randfile, authlevel, ca, cert,
		privatekey ) != 0 ) {
	    /* error message printed in tls_setup */
	    exit( 2 );
	}
	if ( tls_client_start( sn, host, authlevel ) != 0 ) {
	    /* error message printed in tls_cleint_starttls */
	    exit( 2 );
	}
    }

    /* Chack/get correct base command file */
    switch( check( sn, "COMMAND", NULL )) { 
    case 0:
	break;
    case 1:
	change++;
	if ( !update ) {
	    goto done;
	}
	break;
    case 2:
	exit( 2 );
    }

    if ( read_kfile( base_kfile ) != 0 ) {
	exit( 2 );
    }

    /* Parse any included command files */
    while (( node = list_pop_head( kfile_list )) != NULL ) {
	if ( read_kfile( node->n_path ) != 0 ) {
	    exit( 2 );
	}
	free( node );
    }

    if ( special_list->l_count > 0 ) {
	if ( createspecial( sn, special_list ) != 0 ) {
	    exit( 2 );
	}

	if ( snprintf( path, MAXPATHLEN, "%sspecial.T", kdir )
		>= MAXPATHLEN ) {
	    fprintf( stderr, "path too long: %sspecial.T\n", kdir );
	    exit( 2 );
	}
	if ( snprintf( tempfile, MAXPATHLEN, "%sspecial.T.%i", kdir,
		getpid()) >= MAXPATHLEN ) {
	    fprintf( stderr, "path too long: %sspecial.T.%i\n", kdir,
		    (int)getpid());
	    exit( 2 );
	}
	/* get file sizes */
	if ( stat( path, &lst ) != 0 ) {
	    if ( errno == ENOENT ) {
		/* special.T did not exist */
		if ( update ) { 
		    if ( rename( tempfile, path ) != 0 ) {
			fprintf( stderr, "rename failed: %s %s\n", tempfile,
			    path );
			exit( 2 );
		    }
		    if ( !quiet ) printf( "%s: created\n", path ); 
		    change++;
		} else {
		    /* special.T not updated */
		    if ( unlink( tempfile ) !=0 ) {
			perror( tempfile );
			exit( 2 );
		    }
		}
		goto done;
	    }
	    perror( path );
	    exit( 2 );
	}
	if ( stat( tempfile, &tst ) != 0 ) {
	    perror( tempfile );
	    exit( 2 );
	}
	/* get checksums */
	if ( cksum ) {
	    if ( do_cksum( path, lcksum ) < 0 ) {
		perror( path );
		exit( 2 );
	    }
	    if ( do_cksum( tempfile, tcksum ) < 0 ) {
		perror( tempfile );
		exit( 2 );
	    }
	}

	/*
	 * Without checksums we must assume that the special 
	 * transcript has changed since there is no way to
	 * verify its contents
	 */
	/* Special exists */
	if ( !cksum ||
		(( tst.st_size != lst.st_size ) ||
		( strcmp( tcksum, lcksum) != 0 ))) {
	    change++;

	    if ( update ) {
		if ( rename( tempfile, path ) != 0 ) {
		    fprintf( stderr, "rename failed: %s %s\n", tempfile,
			    path );
		    exit( 2 );
		}
		if ( !quiet ) printf( "%s: updated\n", path ); 
	    } else {
		if ( unlink( tempfile ) !=0 ) {
		    perror( tempfile );
		    exit( 2 );
		}
	    }
	} else {
	    /* special.T not updated */
	    if ( unlink( tempfile ) !=0 ) {
		perror( tempfile );
		exit( 2 );
	    }
	}
    }

    if (( closesn( sn )) !=0 ) {
	fprintf( stderr, "can not close sn\n" );
	exit( 2 );
    }

done:
    if ( change ) {
	exit( 1 );
    } else {
	if ( !quiet ) printf( "No updates needed\n" );
	exit( 0 );
    }
}

    int
read_kfile( char * kfile )
{
    int		ac;
    char	**av;
    char        line[ MAXPATHLEN ];
    char	path[ MAXPATHLEN ];
    ACAV	*acav;
    FILE	*f;

    if (( acav = acav_alloc( )) == NULL ) {
	perror( "acav_alloc" );
	return( -1 );
    }

    if (( f = fopen( kfile, "r" )) == NULL ) {
	perror( kfile );
	return( -1 );
    }

    while ( fgets( line, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	ac = acav_parse( acav, line, &av );

	if (( ac == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	if ( ac != 2 ) {
	    fprintf( stderr, "%s: %d: invalid command line\n",
		kfile, linenum );
	    goto error;
	}

	switch( *av[ 0 ] ) {
	case 'k':
	    if ( snprintf( path, MAXPATHLEN, "%s%s", kdir,
		    av[ 1 ] ) >= MAXPATHLEN ) {
		fprintf( stderr, "path too long: %s%s\n", kdir, av[ 1 ] );
		goto error;
	    }
	    if ( !list_check( kfile_seen, path )) {
		if ( list_insert_tail( kfile_list, path ) != 0 ) {
		    perror( "list_insert_tail" );
		    goto error;
		}
		if ( list_insert_tail( kfile_seen, path ) != 0 ) {
		    perror( "list_insert_tail" );
		    goto error;
		}
	    }
	    switch( check( sn, "COMMAND", av[ ac - 1] )) {
	    case 0:
		break;
	    case 1:
		change++;
		if ( !update ) {
		    goto done;
		}
		break;
	    case 2:
		goto error;
	    }
	    break;

	case 's':
	    if ( list_insert( special_list, av[ 1 ] ) != 0 ) {
		perror( "list_insert" );
		exit( 2 );
	    }
	    continue;
	    
	case 'p':
	case 'n':
	    switch( check( sn, "TRANSCRIPT", av[ ac - 1] )) {
	    case 0:
		break;
	    case 1:
		change++;
		if ( !update ) {
		    goto done;
		}
		break;
	    case 2:
		exit( 2 );
	    }
	    break;
	}
    }
    if ( ferror( f )) {
	perror( "fgets" );
	return( -1 );
    }

done:
    if ( fclose( f ) != 0 ) {
	perror( "fclose" );
	return( -1 );
    }
    if ( acav_free( acav ) != 0 ) {
	perror( "acav_free" );
	return( -1 );
    }
    return( 0 );

error:
    fclose( f );
    acav_free( acav );
    return( -1 );
}
