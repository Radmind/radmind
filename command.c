/*
 * Copyright (c) 2003, 2007 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <ctype.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#ifdef HAVE_LIBPAM
    #ifdef HAVE_PAM_PAM_APPL_H
	#include <pam/pam_appl.h>
    #elif HAVE_SECURITY_PAM_APPL_H
	#include <security/pam_appl.h>
    #else /* HAVE_PAM_ not defined */
	die die die
    #endif /* HAVE_PAM_* */
#endif /* HAVE_LIBPAM */

extern SSL_CTX  *ctx;

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include <snet.h>

#include "applefile.h"
#include "base64.h"
#include "command.h"
#include "argcargv.h"
#include "cksum.h"
#include "code.h"
#include "list.h"
#include "wildcard.h"
#include "largefile.h"
#include "mkdirs.h"
#include "connect.h"

#define RADMIND_MAX_INCLUDE_DEPTH	10

#define	DEFAULT_MODE 0444
#define DEFAULT_UID     0
#define DEFAULT_GID     0

#define K_COMMAND 1
#define K_TRANSCRIPT 2
#define K_SPECIAL 3
#define K_FILE 4

int 		read_kfile( SNET *sn, char *kfile );

int		f_quit( SNET *, int, char *[] );
int		f_noop( SNET *, int, char *[] );
int		f_help( SNET *, int, char *[] );
int		f_stat( SNET *, int, char *[] );
int		f_retr( SNET *, int, char *[] );
int		f_stor( SNET *, int, char *[] );
int		f_noauth( SNET *, int, char *[] );
int		f_notls( SNET *, int, char *[] );
int		f_starttls( SNET *, int, char *[] );
int		f_repo( SNET *, int, char *[] );
#ifdef HAVE_LIBPAM
int		f_login( SNET *, int, char *[] );
int 		exchange( int num_msg, struct pam_message **msgm,
		    struct pam_response **response, void *appdata_ptr );
#endif /* HAVE_LIBPAM */
#ifdef HAVE_ZLIB
int		f_compress( SNET *, int, char *[] );
#endif /* HAVE_ZLIB */


char		*user = NULL;
char		*password = NULL;
char		*remote_host = NULL;
char		*remote_addr = NULL;
char		*remote_cn = NULL;
char		special_dir[ MAXPATHLEN ];
char		command_file[ MAXPATHLEN ];
char		upload_xscript[ MAXPATHLEN ];
const EVP_MD    *md = NULL;
struct list	*access_list = NULL;
int		ncommands = 0;
int		authorized = 0;
int		prevstor = 0;
int		case_sensitive = 1;
char		hostname[ MAXHOSTNAMELEN ];
#ifdef HAVE_ZLIB
int		max_zlib_level = 0;
#endif /* HAVE_ZLIB */

extern int	debug;

extern int 	authlevel;
extern int 	checkuser;

struct command	notls[] = {
    { "QUIT",		f_quit },
    { "NOOP",		f_noop },
    { "HELP",		f_help },
    { "STATus",		f_notls },
    { "RETRieve",	f_notls },
    { "STORe",		f_notls },
    { "STARttls",       f_starttls },
    { "REPOrt",         f_notls },
#ifdef HAVE_LIBPAM
    { "LOGIn",       	f_notls },
#endif /* HAVE_LIBPAM */
#ifdef HAVE_ZLIB
    { "COMPress",	f_notls },
#endif /* HAVE_ZLIB */
};

struct command	noauth[] = {
    { "QUIT",		f_quit },
    { "NOOP",		f_noop },
    { "HELP",		f_help },
    { "STATus",		f_noauth },
    { "RETRieve",	f_noauth },
    { "STORe",		f_noauth },
    { "REPOrt",         f_noauth },
#ifdef HAVE_LIBPAM
    { "LOGIn",       	f_noauth },
#endif /* HAVE_LIBPAM */
#ifdef HAVE_ZLIB
    { "COMPress",	f_noauth },
#endif /* HAVE_ZLIB */
};

struct command	auth[] = {
    { "QUIT",		f_quit },
    { "NOOP",		f_noop },
    { "HELP",		f_help },
    { "STATus",		f_stat },
    { "RETRieve",	f_retr },
    { "STORe",		f_stor },
    { "STARttls",       f_starttls },
    { "REPOrt",         f_repo },
#ifdef HAVE_LIBPAM
    { "LOGIn",       	f_login },
#endif /* HAVE_LIBPAM */
#ifdef HAVE_ZLIB
    { "COMPress",	f_compress },
#endif /* HAVE_ZLIB */
};

struct command *commands  = NULL;

    int
f_quit( SNET *sn, int ac, char **av )
{
    snet_writef( sn, "%d QUIT OK, closing connection\r\n", 201 );
#ifdef HAVE_ZLIB
    if ( debug && max_zlib_level > 0 ) print_stats( sn );
#endif /* HAVE_ZLIB */
    exit( 0 );
}

    int
f_noop( SNET *sn, int ac, char **av )
{
    snet_writef( sn, "%d NOOP OK\r\n", 202 );
    return( 0 );
}

    int
f_help( SNET *sn, int ac, char **av )
{
    snet_writef( sn, "%d What is this, SMTP?\r\n", 203 );
    return( 0 );
}

    int
f_noauth( SNET *sn, int ac, char **av )
{
    snet_writef( sn, "%d No access for %s\r\n", 500, remote_host );
    exit( 1 );
}

    int
f_notls( SNET *sn, int ac, char **av )
{
    snet_writef( sn, "%d Must issue a STARTTLS command first\r\n", 530 );
    exit( 1 );
}

    int
keyword( int ac, char *av[] )
{
    /*
     * For now let's always check the biggest max,
     * and later we can worry about command specific
     * checks or no. remote_host is global. +5 is for
     * "/tmp\0"
     */

    int		rc;

    if ( ac < 2 ) { 
	return( -1 );
    }

    if ( strcasecmp( av[ 1 ], "COMMAND" ) == 0 ) {
	if ( ac > 3 ) {
	    return( -1 );
	}
	if ( ac == 2 ) {
	    if ( strlen( command_file + 5 ) > MAXPATHLEN )  {
		syslog( LOG_WARNING, "[tmp]/%s longer than MAXPATHLEN",
			command_file );
		return( -1 );
	    }
	}

	return( K_COMMAND );
    }

    if ( strcasecmp( av[ 1 ], "SPECIAL" ) == 0 ) {
	if ( ac != 3 ) {
	    return( -1 );
	}

	if ( strlen( av[ 1 ] ) + strlen( av[ 2 ] ) +
		strlen( remote_host ) + 5 > MAXPATHLEN ) {
	    syslog( LOG_WARNING,
		    "Overflow attempt: %s/%s-%s longer than MAXPATHLEN",
		    av[ 1 ], av[ 2 ], remote_host );
	    return( -1 );
	}
	rc = K_SPECIAL;

    } else if ( strcasecmp( av[ 1 ], "TRANSCRIPT" ) == 0 ) {
	if ( ac != 3 ) {
	    return( -1 );
	}

	if ( strlen( av[ 1 ] ) + strlen( av[ 2 ] ) + 5 > MAXPATHLEN ) {
	    syslog( LOG_WARNING, "[tmp]/%s/%s longer than MAXPATHLEN",
		    av[ 1 ], av [ 2 ] );
	    return( -1 );
	}
	rc = K_TRANSCRIPT;

    } else if ( strcasecmp( av[ 1 ], "FILE" ) == 0 ) {
	if ( ac != 4 ) {
	    return( -1 );
	}

	/* Check for leading ../ */
	if ( strncmp( av[ 3 ], "../", strlen( "../" )) == 0 ) {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", av[ 3 ] );
	    return( -1 );
	}

	/* Check for internal /../ */
	if ( strstr( av[ 3 ], "/../" ) != NULL ) {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", av[ 3 ] );
	    return( -1 );
	}

	if ( strlen( av[ 1 ] ) + strlen( av[ 2 ] ) +
		strlen( av[ 3 ] ) + 5 > MAXPATHLEN ) {
	    syslog( LOG_WARNING,
		    "Overflow attempt: %s/%s/%s longer than MAXPATHLEN",
		    av[ 1 ], av[ 2 ], av[ 3 ] );
	    return( -1 );
	}

	rc = K_FILE;

    } else {
	return( -1 );
    }

    if ( strstr( av[ 2 ], "../" ) != NULL ) {
	syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", av[ 2 ] );
	return( -1 );
    }

    return( rc );
}

    int
f_retr( SNET *sn, int ac, char **av )
{

    ssize_t		readlen;
    struct stat		st;
    struct timeval	tv;
    char		buf[8192];
    char		path[ MAXPATHLEN ];
    char		*d_path, *d_tran;
    int			fd;

    switch ( keyword( ac, av )) {
    case K_COMMAND:
	if ( ac == 2 ) { 

	    if ( snprintf( path, MAXPATHLEN, "command/%s", command_file )
		    >= MAXPATHLEN ) {
		syslog( LOG_ERR, "f_retr: command/%s: path too long",
		    command_file );
		snet_writef( sn, "%d Path too long\r\n", 540 );
		return( 1 );
	    }
	} else {
	    if (( d_path = decode( av[ 2 ] )) == NULL ) {
		syslog( LOG_ERR, "f_retr: decode: buffer too small" );
		snet_writef( sn, "%d Line too long\r\n", 540 );
		return( 1 );
	    } 

	    /* Check for access */
	    if ( !list_check( access_list, d_path )) {
		syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s",
		    d_path );
		snet_writef( sn, "%d No access for %s\r\n", 540, d_path );
		return( 1 );
	    }

	    if ( snprintf( path, MAXPATHLEN, "command/%s", d_path )
		    >= MAXPATHLEN ) {
		syslog( LOG_ERR, "f_retr: command path too long" );
		snet_writef( sn, "%d Path too long\r\n", 540 );
		return( 1 );
	    }
	}
	break;

    case K_TRANSCRIPT:
	if (( d_tran = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_retr: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	/* Check for access */
	if ( !list_check( access_list, d_tran )) {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", d_tran );
	    snet_writef( sn, "%d No access for %s\r\n", 540, d_tran );
	    return( 1 );
	}

	if ( snprintf( path, MAXPATHLEN, "transcript/%s", d_tran )
		>= MAXPATHLEN ) {
	    syslog( LOG_ERR, "f_retr: transcript path too long" );
	    snet_writef( sn, "%d Path too long\r\n", 540 );
	    return( 1 );
	}
	break;

    case K_SPECIAL:
	if (( d_path = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_retr: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	if ( snprintf( path, MAXPATHLEN, "%s/%s", special_dir, d_path )
		>= MAXPATHLEN ) {
	    syslog( LOG_ERR, "f_retr: special path too long" );
	    snet_writef( sn, "%d Path too long\r\n", 540 );
	    return( 1 );
	}

	break;

    case K_FILE:
	if (( d_path = decode( av[ 3 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_retr: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 
	if (( d_path = strdup( d_path )) == NULL ) {
	    syslog( LOG_ERR, "f_retr: strdup: %s: %m", d_path );
	    return( -1 );
	}
	if (( d_tran = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_retr: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	/* Check for access */
	if ( !list_check( access_list, d_tran )) {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", d_tran );
	    snet_writef( sn, "%d No access for %s:%s\r\n", 540, d_tran,
		d_path );
	    return( 1 );
	}

	if ( snprintf( path, MAXPATHLEN, "file/%s/%s", d_tran, d_path )
		>= MAXPATHLEN ) {
	    syslog( LOG_ERR, "f_retr: file path too long" );
	    snet_writef( sn, "%d Path too long\r\n", 540 );
	    return( 1 );
	}
	free( d_path );
	break;

    default:
	snet_writef( sn, "%d RETR Syntax error\r\n", 540 );
	return( 1 );
    }

    if (( fd = open( path, O_RDONLY, 0 )) < 0 ) {
    	syslog( LOG_ERR, "open: %s: %m", path );
	snet_writef( sn, "%d Unable to access %s.\r\n", 543, path );
	return( 1 );
    }
    
    /* dump file info */

    if ( fstat( fd, &st ) < 0 ) { 
	syslog( LOG_ERR, "f_retr: fstat: %m" );
	snet_writef( sn, "%d Access Error: %s\r\n", 543, path );
	if ( close( fd ) < 0 ) {
	    syslog( LOG_ERR, "close: %m" );
	    return( -1 );
	}
	return( 1 );
    }

    /*
     * Here's a problem.  Do we need to add long long support to
     * snet_writef?
     */
    snet_writef( sn, "240 Retrieving file\r\n%" PRIofft "d\r\n", st.st_size );

    /* dump file */

    while (( readlen = read( fd, buf, sizeof( buf ))) > 0 ) {
	tv.tv_sec = 60 ;
	tv.tv_usec = 0;
	if ( snet_write( sn, buf, readlen, &tv ) != readlen ) {
	    syslog( LOG_ERR, "snet_write: %m" );
	    return( -1 );
	}
    }

    if ( readlen < 0 ) {
	syslog( LOG_ERR, "read: %m" );
	return( -1 );
    }

    snet_writef( sn, ".\r\n" );

    if ( close( fd ) < 0 ) {
        syslog( LOG_ERR, "close: %m" );
	return( -1 );
    }

    syslog( LOG_DEBUG, "f_retr: 'file' %s retrieved", path );

    return( 0 );
}

/* looks for special file info in transcripts */
    char **
special_t( char *sp_path, char *remote_path )
{
    FILE		*fs = NULL;
    int			i, ac, len, ln;
    char		**av = NULL;
    char		*paths[ 4 ] = { NULL };
    char		*p;
    char		sp_t[ MAXPATHLEN ];
    static char		line[ MAXPATHLEN ];

    /*
     * in order, we look for special file transcript lines in the
     * following locations:
     *
     *      - A transcript in the same directory and with the same name
     *	      as the special file, but with a ".T" extension.
     *
     *      - A transcript named "<remote_id>.T" in the same directory as
     *        the client's special file directory root.
     *
     *      - /var/radmind/transcript/special.T
     *
     * if no matching transcript line is found, default metadata is
     * returned to the client (type: f; mode: 0444; owner: 0; group: 0).
     */
    paths[ 0 ] = sp_path;
    paths[ 1 ] = special_dir;
    paths[ 2 ] = "transcript/special.T";
    paths[ 3 ] = NULL;

    for ( i = 0; paths[ i ] != NULL; i++ ) {
	if (( p = strrchr( paths[ i ], '.' )) != NULL
		&& strcmp( p, ".T" ) == 0 ) {
	    if ( strlen( paths[ i ] ) >= MAXPATHLEN ) {
		syslog( LOG_WARNING, "special_t: path \"%s\" too long",
			paths[ i ] );
		continue;
	    }
	    strcpy( sp_t, paths[ i ] );
	} else if ( snprintf( sp_t, MAXPATHLEN, "%s.T",
			      paths[ i ] ) >= MAXPATHLEN ) {
	    syslog( LOG_WARNING, "special_t: path \"%s.T\" too long", sp_path );
	    continue;
	}

	if (( fs = fopen( sp_t, "r" )) == NULL ) {
	    continue;
	}

	ln = 0;
	while ( fgets( line, MAXPATHLEN, fs ) != NULL ) {
	    ln++;
	    len = strlen( line );
	    if (( line[ len - 1 ] ) != '\n' ) {
		syslog( LOG_ERR, "special_t: %s: line %d too long", sp_t, ln );
		break;
	    }

	    /* only files and applefiles allowed */
	    if ( strncmp( line, "f ", strlen( "f " )) != 0 &&
			strncmp( line, "a ", strlen( "a " )) != 0 ) {
		continue;
	    }
	    if (( ac = argcargv( line, &av )) != 8 ) {
		syslog( LOG_WARNING, "special_t: %s: line %d: "
			"bad transcript line", sp_t, ln );
		continue;
	    }

	    if ( strcmp( av[ 1 ], remote_path ) == 0 ) { 
		(void)fclose( fs );
		return( av );
	    }
	}

	if ( fclose( fs ) != 0 ) {
	    syslog( LOG_WARNING, "special_t: fclose %s: %m", sp_t );
	}
	fs = NULL;
    }
    if ( fs != NULL ) {
	(void)fclose( fs );
    }
    
    return( NULL );
}

    int
f_stat( SNET *sn, int ac, char *av[] )
{

    char 		path[ MAXPATHLEN ];
    char		cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    struct stat		st;
    int			key;
    char		*enc_file, *d_tran, *d_path;

    switch ( key = keyword( ac, av )) {
    case K_COMMAND:
	if ( ac == 2 ) { 
	    if ( snprintf( path, MAXPATHLEN, "command/%s", command_file )
		    >= MAXPATHLEN ) {
		syslog( LOG_ERR, "f_stat: command/%s: path too long",
		    command_file );
		snet_writef( sn, "%d Path too long\r\n", 540 );
		return( 1 );
	    }
	} else {
	    if (( d_path = decode( av[ 2 ] )) == NULL ) {
		syslog( LOG_ERR, "f_stat: decode: buffer too small" );
		snet_writef( sn, "%d Line too long\r\n", 540 );
		return( 1 );
	    } 

	    /* Check for access */
	    if ( !list_check( access_list, d_path )) {
		syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s",
		    d_path );
		snet_writef( sn, "%d No access for %s\r\n", 540, d_path );
		return( 1 );
	    }

	    if ( snprintf( path, MAXPATHLEN, "command/%s", d_path )
		    >= MAXPATHLEN ) {
		syslog( LOG_ERR, "f_stat: command path too long" );
		snet_writef( sn, "%d Path too long\r\n", 540 );
		return( 1 );
	    }
	}
	break;

    case K_TRANSCRIPT:
	if (( d_tran = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_stat: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	/* Check for access */
	if ( !list_check( access_list, d_tran )) {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", d_tran );
	    snet_writef( sn, "%d No access for %s\r\n", 540, d_tran );
	    return( 1 );
	}

	if ( snprintf( path, MAXPATHLEN, "transcript/%s", d_tran )
		>= MAXPATHLEN ) {
	    syslog( LOG_ERR, "f_stat: transcript path too long" );
	    snet_writef( sn, "%d Path too long\r\n", 540 );
	    return( 1 );
	}
	break;

    case K_SPECIAL:
	if (( d_path = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_stat: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	if ( snprintf( path, MAXPATHLEN, "%s/%s", special_dir, d_path) 
		>= MAXPATHLEN ) {
	    syslog( LOG_ERR, "f_stat: special path too long" );
	    snet_writef( sn, "%d Path too long\r\n", 540 );
	    return( 1 );
	}
	break;

    default:
	snet_writef( sn, "%d STAT Syntax error\r\n", 530 );
	return( 1 );
    }
        
    syslog( LOG_DEBUG, "f_stat: returning infomation for %s", path );

    if ( stat( path, &st ) < 0 ) {
        syslog( LOG_ERR, "f_stat: stat: %m" );
	snet_writef( sn, "%d Access Error: %s\r\n", 531, path );
	return( 1 );
    }

    /* XXX cksums here, totally the wrong place to do this! */
    OpenSSL_add_all_digests();
    md = EVP_get_digestbyname( "sha1" );
    if ( !md ) {
	/* XXX */
	fprintf( stderr, "%s: unsupported checksum\n", "sha1" );
	exit( 1 );
    }
    if ( do_cksum( path, cksum_b64 ) < 0 ) {
	syslog( LOG_ERR, "do_cksum: %s: %m", path );
	snet_writef( sn, "%d Checksum Error: %s: %m\r\n", 500, path );
	return( 1 );
    }

    snet_writef( sn, "%d Returning STAT information\r\n", 230 );
    switch ( key ) {
    case K_COMMAND:
	if ( ac == 2 ) {
	    snet_writef( sn, RADMIND_STAT_FMT,
		"f", "command", DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID,
		st.st_mtime, st.st_size, cksum_b64 );
	} else {
	    snet_writef( sn, RADMIND_STAT_FMT,
		"f", av[ 2 ], DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID,
		st.st_mtime, st.st_size, cksum_b64 );
	}
	return( 0 );
        
		    
    case K_TRANSCRIPT:
	snet_writef( sn, RADMIND_STAT_FMT,
		"f", av[ 2 ], 
		DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID,
		st.st_mtime, st.st_size, cksum_b64 );
	return( 0 );
    
    case K_SPECIAL:
	/*
	 * store value of av[ 2 ], because argcargv will be called
	 * from special_t(), and that will blow away the current values
	 * for av[ 2 ].
	 */
	if (( enc_file = strdup( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_stat: strdup: %s %m", av[ 2 ] );
	    return( -1 );
	}

	if (( av = special_t( path, enc_file )) == NULL ) {
	    /* no special transcript match found, return defaults. */
	    snet_writef( sn, RADMIND_STAT_FMT,
		    "f", enc_file, 
		    DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, 
		    st.st_mtime, st.st_size, cksum_b64 );
	    free( enc_file );
	    return( 0 );
	}
        /*
         * Cannot use RADMIND_STAT_FMT shorthand here, since custom
         * permission, user and group information are strings.
         */
        snet_writef( sn, "%s %s %s %s %s %" PRItimet "d %" PRIofft "d %s\r\n",
		av[ 0 ], enc_file,
		av[ 2 ], av[ 3 ], av[ 4 ],
		st.st_mtime, st.st_size, cksum_b64 );

	free( enc_file );
	return( 0 );

    default:
        return( 1 );
    }
}

    int
f_stor( SNET *sn, int ac, char *av[] )
{
    char 		*sizebuf;
    char		xscriptdir[ MAXPATHLEN ];
    char		upload[ MAXPATHLEN ];
    char		buf[ 8192 ];
    char		*line;
    char		*d_tran, *d_path;
    int			fd;
    int			zero = 0;
    off_t		len;
    ssize_t		rc;
    struct timeval	tv;
    struct protoent	*proto;

    if ( !prevstor ) {
	/* Turn off TCP_NODELAY for stores */
	if (( proto = getprotobyname( "tcp" )) == NULL ) {
	    syslog( LOG_ERR, "f_stor: getprotobyname: %m" );
	    return( -1 );
	}

	if ( setsockopt( snet_fd( sn ), proto->p_proto, TCP_NODELAY, &zero,
		sizeof( zero )) != 0 ) {
	    syslog( LOG_ERR, "f_stor: snet_setopt: %m" );
	    return( -1 );
	}
	prevstor = 1;
    }

    if ( checkuser && ( !authorized )) {
	snet_writef( sn, "%d Not logged in\r\n", 551 );
	exit( 1 );
    }
    /* decode() uses static mem, so strdup() */
    if (( d_tran = decode( av[ 2 ] )) == NULL ) {
	syslog( LOG_ERR, "f_stor: decode: buffer too small" );
	snet_writef( sn, "%d Line too long\r\n", 540 );
	return( 1 );
    } 
    if (( d_tran = strdup( d_tran )) == NULL ) {
	syslog( LOG_ERR, "f_stor: strdup: %s: %m", d_tran );
	return( -1 );
    }

    switch ( keyword( ac, av )) {

    case K_TRANSCRIPT:
        if ( snprintf( xscriptdir, MAXPATHLEN, "tmp/file/%s", d_tran )
		>= MAXPATHLEN ) {
	    syslog( LOG_ERR, "f_stor: xscriptdir path too long" );
	    snet_writef( sn, "%d Path too long\r\n", 540 );
	    return( 1 );
	}
        if ( snprintf( upload, MAXPATHLEN, "tmp/transcript/%s", d_tran )
		>= MAXPATHLEN ) {
	    syslog( LOG_ERR, "f_stor: upload path too long" );
	    snet_writef( sn, "%d Path too long\r\n", 540 );
	    return( 1 );
	}

	/* keep encoded transcript name, since it will just be
	 * used later to compare in a stor file.
	 */
	if ( strlen( av[ 2 ] ) >= MAXPATHLEN ) {
	    syslog( LOG_ERR, "f_stor: upload_xscript path too long" );
	    snet_writef( sn, "%d Path too long\r\n", 540 );
	    return( 1 );
	}
	strcpy( upload_xscript, av[ 2 ] );

	/* make the directory for the files of this xscript to live in. */
	if ( mkdir( xscriptdir, 0777 ) < 0 ) {
	    if ( errno == EEXIST ) {
	        snet_writef( sn, "%d Transcript exists\r\n", 551 );
		exit( 1 );
	    }
	    snet_writef( sn, "%d %s: %s\r\n",
		    551, xscriptdir, strerror( errno ));
	    exit( 1 );
	}
	break;

    case K_FILE:
	/* client must have provided a transcript name before giving 
	 * files in that transcript
	 */
	if (( strcmp( upload_xscript, av[ 2 ] ) != 0 )) {
	    snet_writef( sn, "%d Incorrect Transcript %s\r\n", 552, av[ 2 ] );
	    exit( 1 );
	}

	/* decode() uses static mem, so strdup() */
	if (( d_path = decode( av[ 3 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_stor: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 
	if (( d_path = strdup( d_path )) == NULL ) {
	    syslog( LOG_ERR, "f_stor: strdup: %s: %m", d_path );
	    return( -1 );
	}

	if ( d_path[ 0 ] == '/' ) {
	    if ( snprintf( upload, MAXPATHLEN, "tmp/file/%s%s", d_tran,
		    d_path ) >= MAXPATHLEN ) {
		syslog( LOG_ERR, "f_stor: upload path too long" );
		snet_writef( sn, "%d Path too long\r\n", 540 );
		return( 1 );
	    }
	} else {
	    if ( snprintf( upload, MAXPATHLEN, "tmp/file/%s/%s", d_tran,
		    d_path ) >= MAXPATHLEN ) {
		syslog( LOG_ERR, "f_stor: upload path too long" );
		snet_writef( sn, "%d Path too long\r\n", 540 );
		return( 1 );
	    }
	}
	free( d_path );
	free( d_tran );
	break;

    default:
        snet_writef( sn, "%d STOR Syntax error\r\n", 550 );
	exit( 1 ); 
    }

    if (( fd = open( upload, O_CREAT|O_EXCL|O_WRONLY, 0666 )) < 0 ) {
	if ( mkdirs( upload ) < 0 ) {
	    syslog( LOG_ERR, "f_stor: mkdir: %s: %m", upload );
	    snet_writef( sn, "%d %s: %s\r\n", 555, upload, strerror( errno ));
	    exit( 1 );
	}
	if (( fd = open( upload, O_CREAT|O_EXCL|O_WRONLY, 0666 )) < 0 ) {
	    syslog( LOG_ERR, "f_stor: open: %s: %m", upload );
	    snet_writef( sn, "%d %s: %s\r\n", 555, upload, strerror( errno ));
	    exit( 1 );
	}
    }


    snet_writef( sn, "%d Storing file\r\n", 350 );

    tv.tv_sec = 60;
    tv.tv_usec = 0;
    if ( ( sizebuf = snet_getline( sn, &tv ) ) == NULL ) {
	syslog( LOG_ERR, "f_stor: snet_getline: %m" );
	return( -1 );
    }
    /* Will there be a limit? */
    len = strtoofft( sizebuf, NULL, 10 );

    for ( ; len > 0; len -= rc ) {
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	if (( rc = snet_read(
		sn, buf, MIN( len, sizeof( buf )), &tv )) <= 0 ) {
	    if ( snet_eof( sn )) {
		syslog( LOG_ERR, "f_stor: snet_read: eof" );
	    } else {
		syslog( LOG_ERR, "f_stor: snet_read: %m" );
	    }
	    return( -1 );
	}

	if ( write( fd, buf, rc ) != rc ) {
	    snet_writef( sn, "%d %s: %s\r\n", 555, upload, strerror( errno ));
	    exit( 1 );
	}
    }

    if ( len != 0 ) {
	syslog( LOG_ERR, "f_stor: len is %" PRIofft "d", len );
	snet_writef( sn, "%d %s: internal error!\r\n", 555, upload );
	exit( 1 );
    }

    if ( close( fd ) < 0 ) {
	snet_writef( sn, "%d %s: %s\r\n", 555, upload, strerror( errno ));
	exit( 1 );
    }

    syslog( LOG_DEBUG, "f_stor: file %s stored", upload );

    tv.tv_sec = 60;
    tv.tv_usec = 0;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
        syslog( LOG_ERR, "f_stor: snet_getline: %m" );
	return( -1 );
    }

    /* make sure client agrees we're at the end */
    if ( strcmp( line, "." ) != 0 ) {
        syslog( LOG_ERR, "f_stor: line is: %s", line );
	snet_writef( sn, "%d Length doesn't match sent data %s\r\n",
		555, upload );
	(void)unlink( upload );
	exit( 1 );
    }

    snet_writef( sn, "%d File stored\r\n", 250 );
    return( 0 );
}

    int
f_repo( SNET *sn, int ac, char **av )
{
    char			*cn = "-";
    char			*d_msg;

    if ( ac != 3 ) {
	snet_writef( sn, "%d Syntax error (invalid parameters)\r\n", 501 );
	return( 1 );
    }

    if (( d_msg = decode( av[ 2 ] )) == NULL ) {
	syslog( LOG_ERR, "f_repo: decode: buffer too small" );
	snet_writef( sn, "%d Syntax error (invalid parameter)\r\n", 501 );
	return( 1 );
    }

    if ( remote_cn != NULL ) {
	cn = remote_cn;
    }

    syslog( LOG_NOTICE, "report %s %s %s %s %s %s",
		remote_host, remote_addr,
		cn, "-", /* reserve for user specified ID, e.g. sasl */
		av[ 1 ], d_msg );

    snet_writef( sn, "%d Report successful\r\n", 215 );
    
    return( 0 );
}

    int
f_starttls( SNET *sn, int ac, char **av )
{
    int                         rc;
    X509                        *peer;
    char                        buf[ 1024 ];

    if ( ac != 1 ) {  
        snet_writef( sn, "%d Syntax error (no parameters allowed)\r\n", 501 );
        return( 1 );
    } else {
	snet_writef( sn, "%d Ready to start TLS\r\n", 220 );
    }

    /* We get here when the client asks for TLS with the STARTTLS verb */
    /*
     * Client MUST NOT attempt to start a TLS session if a TLS     
     * session is already active.  No mention of what to do if it does...
     *
     * Once STARTTLS has succeeded, the STARTTLS verb is no longer valid
     */

    /*
     * Begin TLS
     */
    /* This is where the TLS start */
    /* At this point the client is also starting TLS */
    /* 1 is for server, 0 is client */
    if (( rc = snet_starttls( sn, ctx, 1 )) != 1 ) {
        syslog( LOG_ERR, "f_starttls: snet_starttls: %s",
                ERR_error_string( ERR_get_error(), NULL ) );
        snet_writef( sn, "%d SSL didn't work error! XXX\r\n", 501 );
        return( 1 );
    }

    if ( authlevel >= 2 ) {
	if (( peer = SSL_get_peer_certificate( sn->sn_ssl ))
		== NULL ) {
	    syslog( LOG_ERR, "no peer certificate" );
	    return( -1 );
	}

	syslog( LOG_INFO, "CERT Subject: %s\n",
	    X509_NAME_oneline( X509_get_subject_name( peer ), buf,
	    sizeof( buf )));

	X509_NAME_get_text_by_NID( X509_get_subject_name( peer ),
	    NID_commonName, buf, sizeof( buf ));
	if (( remote_cn = strdup( buf )) == NULL ) {
	    syslog( LOG_ERR, "strdup: %m" );
	    X509_free( peer );
	    return( -1 );
	}
	X509_free( peer );
    }

    /* get command file */
    if ( command_k( "config", 0 ) < 0 ) {
	/* Client not in config */
	commands  = noauth;
	ncommands = sizeof( noauth ) / sizeof( noauth[ 0 ] );
    } else {
	/* Client in config */
	commands  = auth;
	ncommands = sizeof( auth ) / sizeof( auth[ 0 ] );

	if ( read_kfile( sn, command_file ) != 0 ) {
	    /* error message given in list_transcripts */
	    exit( 1 );
	}
    }

    return( 0 );
}

#ifdef HAVE_LIBPAM
    int
exchange( int num_msg, struct pam_message **msg,
    struct pam_response **resp, void *appdata_ptr)
{
    int				count = 0;
    struct pam_response		*reply= NULL;

    if ( num_msg <= 0 ) {
	return( PAM_CONV_ERR );
    }

    /*
     * From pam_start man page:
     *
     * "The storage used by pam_response has to be allocated by the
     * application and freed by the PAM modules."
     */

    if (( reply = malloc( num_msg * sizeof( struct pam_response ))) == NULL ) {
	return( PAM_CONV_ERR );
    }

    for ( count = 0; count < num_msg; count++ ) {
	char 	*string = NULL;

	switch( msg[ count ]->msg_style ) {

	case PAM_PROMPT_ECHO_OFF:
	case PAM_PROMPT_ECHO_ON:
	    string = strdup( password );
	    if ( string == NULL ) {
		goto exchange_failed;
	    }
	    break;

	case PAM_TEXT_INFO:
	case PAM_ERROR_MSG:
	    string = NULL;
	    break;

	default:
	    goto exchange_failed;
	}

	reply[ count ].resp = string;
	reply[ count ].resp_retcode = 0;
	string = NULL;
    }

    *resp = reply;
    reply = NULL;

    return( PAM_SUCCESS );

exchange_failed:

    if ( reply ) {
	for ( count = 0; count < num_msg; count++ ) {
	    if ( reply[ count ].resp == NULL ) {
		continue;
	    }
	    free( reply[ count ].resp );
	    reply[ count ].resp = NULL;
	}
	free( reply );
	reply = NULL;
    }

    return( PAM_CONV_ERR );
}

    int
f_login( SNET *sn, int ac, char **av )
{
    int				retval;
    pam_handle_t		*pamh;
    struct pam_conv		pam_conv = {
	(int (*)())exchange,
	NULL
    };

    if ( !checkuser ) {
	snet_writef( sn, "%d login not enabled\r\n", 502 );
	return( 1 );
    }
    /*
    if ( authlevel < 1 ) {
	snet_writef( sn, "%d login requires TLS\r\n", 503 );
	return( 1 );
    }
    */
    if ( ac != 3 ) {  
        snet_writef( sn, "%d Syntax error\r\n", 501 );
        return( 1 );
    }
    if ( user != NULL ) {
	free( user );
	user = NULL;
    }
    if ( password != NULL ) {
	free( password );
	password = NULL;
    }
    if (( user = strdup( av[ 1 ] )) == NULL ) {
	syslog( LOG_ERR, "f_login: strdup: %m" );
	return( -1 );
    }

    if (( password = strdup( av[ 2 ] )) == NULL ) {
	syslog( LOG_ERR, "f_login: strdup: %m" );
	return( -1 );
    }

    if (( retval =  pam_start( "radmind", user, &pam_conv,
	    &pamh )) != PAM_SUCCESS ) {
        syslog( LOG_ERR, "f_login: pam_start: %s\n",
	    pam_strerror( pamh, retval ));
        snet_writef( sn, "%d Authentication Failed\r\n", 535 );
	return( 1 );
    }

    /* is user really user? */
    if (( retval =  pam_authenticate( pamh, PAM_SILENT )) != PAM_SUCCESS ) {
        syslog( LOG_ERR, "f_login: pam_authenticate: %s\n",
	    pam_strerror( pamh, retval ));
        snet_writef( sn, "%d Authentication Failed\r\n", 535 );
	return( 1 );
    }
    free( password );

    /* permitted access? */
    if (( retval = pam_acct_mgmt( pamh, 0 )) != PAM_SUCCESS ) {
        syslog( LOG_ERR, "f_login: pam_acct_mgmt: %s\n",
	    pam_strerror( pamh, retval ));
        snet_writef( sn, "%d Authentication Failed\r\n", 535 );
	return( 1 );
    }

    if (( retval = pam_end( pamh, retval )) != PAM_SUCCESS ) {
        syslog( LOG_ERR, "f_login: pam_end: %s\n",
	    pam_strerror( pamh, retval ));
        snet_writef( sn, "%d Authentication Failed\r\n", 535 );
	return( 1 );
    }
    syslog( LOG_INFO, "%s: successfully logged in\n", user );
    snet_writef( sn, "%d %s successfully logged in\r\n", 205, user );
    authorized = 1;

    return( 0 );
}
#endif /* HAVE_LIBPAM */

#ifdef HAVE_ZLIB
    int
f_compress( SNET *sn, int ac, char **av )
{
    int		level;

    if ( max_zlib_level <= 0 ) {
	syslog( LOG_WARNING, "f_compress: compression not enabled" );
	snet_writef( sn, "501 Compression not enabled\r\n" );
	return( 1 );
    }

    if ( ac != 2 && ac != 3 ) {
	syslog( LOG_WARNING, "f_compress: syntax error" );
	snet_writef( sn, "%d Syntax error\r\n", 501 );
	return( 1 );
    }
    if ( snet_flags( sn ) & SNET_ZLIB ) {
	syslog( LOG_WARNING, "f_compress: compression already enabled" );
	snet_writef( sn, "%d Compression already enabled\r\n", 501 );
	return( 1 );
    }
    if ( strcasecmp( av[ 1 ], "ZLIB" ) == 0 ) {
	if( ac == 3 ) {
	    level = atoi( av[2] );
	    level = MAX( level, 1 );
	    level = MIN( level, max_zlib_level );
	} else {
	    /* If no level given, use max compression */
	    level = max_zlib_level;
	}
	snet_writef( sn, "320 Ready to start ZLIB compression level %d\r\n", level );
	if ( snet_setcompression( sn, SNET_ZLIB, level ) != 0 ) {
	    syslog( LOG_ERR, "f_compress: snet_setcompression failed" );
	    return( -1 );
	}
	snet_writef( sn, "220 ZLIB compression level %d enabled\r\n", level );
    } else {
	syslog( LOG_WARNING, "%s: Unknown compression requested", av[ 1 ] );
	snet_writef( sn, "525 %s: unknown compression type\r\n", av[ 1 ] );
    }
    return( 0 );
}
#endif /* HAVE_ZLIB */


    static char *
match_config_entry( char *entry )
{
    if (( remote_cn != NULL ) && wildcard( entry, remote_cn, 0 )) {
	return( remote_cn );
    } else if ( wildcard( entry, remote_host, 0 )) {
	return( remote_host );
    } else if ( wildcard( entry, remote_addr, 1 )) {
	return( remote_addr );
    }

    return( NULL );
}

/* sets command file for connected host */
    int
command_k( char *path_config, int depth )
{
    SNET	*sn;
    char	**av, *line, *p;
    char	*valid_host;
    char	temp[ MAXPATHLEN ];
    int		ac;
    int		rc = -1;
    int		linenum = 0;

    if (( sn = snet_open( path_config, O_RDONLY, 0, 0 )) == NULL ) {
        syslog( LOG_ERR, "command_k: snet_open: %s: %m", path_config );
	return( -1 );
    }

    while (( line = snet_getline( sn, NULL )) != NULL ) {
	linenum++;

        if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "argvargc: %m" );
	    goto command_k_done;
	}

	if ( ( ac == 0 ) || ( *av[ 0 ] == '#' ) ) {
	    continue;
	}
	if ( ac < 2 ) {
	    syslog( LOG_ERR, "%s: line %d: invalid number of arguments",
			path_config, linenum );
	    continue;
	}
	if ( strcmp( av[ 0 ], "@include" ) == 0 ) {
	    if ( depth >= RADMIND_MAX_INCLUDE_DEPTH ) {
		syslog( LOG_ERR, "%s: line %d: include %s exceeds max depth",
			path_config, linenum, av[ 1 ] );
		goto command_k_done;
	    }
	    if ( ac > 3 ) {
		syslog( LOG_ERR, "%s: line %d: invalid number of arguments",
			path_config, linenum );
		continue;
	    } else if ( ac == 3 ) {
		if ( match_config_entry( av[ 2 ] ) == NULL ) {
		    /* connecting host doesn't match pattern, skip include. */
		    continue;
		}
	    }
	    if ( command_k( av[ 1 ], depth + 1 ) != 0 ) {
		continue;
	    }

	    rc = 0;
	    goto command_k_done;
	}

        if (( ac > 2 ) && ( *av[ 2 ] != '#' )) { 
	    syslog( LOG_ERR, "%s: line %d: invalid number of arguments",
		    path_config, linenum );
	    continue;
	}

	if (( p = strrchr( av[ 1 ], '/' )) == NULL ) {
	    sprintf( special_dir, "special" );
	} else {
	    *p = '\0';
	    if ( snprintf( special_dir, MAXPATHLEN, "special/%s", av[ 1 ] )
		    >= MAXPATHLEN ) {
		syslog( LOG_ERR, "config file: line %d: path too long\n",
		    linenum );
		continue;
	    }
	    *p = '/';
	}

	if (( valid_host = match_config_entry( av[ 0 ] )) != NULL ) {
	    if ( strlen( av[ 1 ] ) >= MAXPATHLEN ) {
		syslog( LOG_ERR,
		    "config file: line %d: command file too long\n", linenum );
		continue;
	    }
	    strcpy( command_file, av[ 1 ] );
	    if ( snprintf( temp, MAXPATHLEN, "%s/%s", special_dir,
		    valid_host ) >= MAXPATHLEN ) {
		syslog( LOG_ERR, "config file: line %d: special dir too long\n",
		    linenum );
		continue;
	    }
	    strcpy( special_dir, temp );
	    rc = 0;
	    goto command_k_done;
	}
    }

    /* If we get here, the host that connected is not in the config
       file. So screw him. */
    syslog( LOG_ERR, "host %s not in config file %s",
		remote_host, path_config );

command_k_done:
    snet_close( sn );
    return( rc );
}

    int
read_kfile( SNET *sn, char *kfile )
{
    int		ac;
    int		linenum = 0;
    char	**av;
    char        line[ MAXPATHLEN ];
    char	path[ MAXPATHLEN ];
    ACAV	*acav;
    FILE	*f;

    if ( snprintf( path, MAXPATHLEN, "command/%s", kfile ) >= MAXPATHLEN ) {
	syslog( LOG_ERR, "read_kfile: command/%s: path too long", kfile );
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	return( -1 );
    }

    if (( acav = acav_alloc( )) == NULL ) {
	syslog( LOG_ERR, "acav_alloc: %m" );
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	return( -1 );
    }

    if (( f = fopen( path, "r" )) == NULL ) {
	syslog( LOG_ERR, "fopen: %s: %m", path );
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	return( -1 );
    }

    while ( fgets( line, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	ac = acav_parse( acav, line, &av );

	if (( ac == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	/* Skip minus lines in command files for now.  Eventually,
	 * the server should not give access to command files, special files
	 * and transcripts that have been ultimately removed with a '-'.
	 * This is difficult as ktcheck reads command files line by line
	 * and will request info on a file that might be removed with a
	 * later '-'.
	 */
	if ( *av[ 0 ] == '-' ) {
	    continue;
	}

	if ( ac != 2 ) {
	    syslog( LOG_ERR, "%s: line %d: invalid number of arguments",
		kfile, linenum );
	    snet_writef( sn,
		"%d Service not available, closing transmission channel\r\n",
		421 );
	    goto error;
	}

	switch( *av[ 0 ] ) {
	case 'k':
	    if ( !list_check( access_list, av[ 1 ] )) {
		if ( list_insert( access_list, av[ 1 ] ) != 0 ) {
		    syslog( LOG_ERR, "list_insert: %m" );
		    snet_writef( sn,
	"%d Service not available, closing transmission channel\r\n", 421 );
		    goto error;
		}
		if ( read_kfile( sn, av[ 1 ] ) != 0 ) {
		    goto error;
		}
	    }
	    break;

	case 'p':
	case 'n':
	    if ( !list_check( access_list, av[ 1 ] )) {
		if ( list_insert( access_list, av[ 1 ] ) != 0 ) {
		    syslog( LOG_ERR, "list_insert: %m" );
		    snet_writef( sn,
	"%d Service not available, closing transmission channel\r\n", 421 );
		    goto error;
		}
	    }
	    break;

	case 's':
	case 'x':
	    break;

	default:
	    syslog( LOG_ERR, "%s: line %d: %c: unknown file type", kfile,
		linenum, *av[ 0 ] );
	    snet_writef( sn,
		"%d Service not available, closing transmission channel\r\n",
		421 );
	    goto error;

	}

	if ( ferror( f )) {
	    syslog( LOG_ERR, "fgets: %m" );
	    snet_writef( sn,
		"%d Service not available, closing transmission channel\r\n",
		421 );
	    goto error;
	}
    }

    if ( fclose( f ) != 0 ) {
	syslog( LOG_ERR, "fclose: %m" );
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	goto error;
    }

    if ( acav_free( acav ) != 0 ) {
	syslog( LOG_ERR, "acav_free: %m" );
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	return( -1 );
    }

    return( 0 );

error:
    fclose( f );
    acav_free( acav );

    return( -1 );
}

    int
cmdloop( int fd, struct sockaddr_in *sin )
{
    SNET		*sn;
    struct hostent	*hp;
    char		*p;
    int			ac, i;
    int			one = 1;
    unsigned int	n;
    char		**av, *line;
    struct timeval	tv;
    extern char		*version;
    extern int		connections;
    extern int		maxconnections;
    extern int		rap_extensions;

    if ( authlevel == 0 ) {
	commands = noauth;
	ncommands = sizeof( noauth ) / sizeof( noauth[ 0 ] );
    } else {
	commands = notls;
	ncommands = sizeof( notls ) / sizeof( notls[ 0 ] );
    }

    if (( sn = snet_attach( fd, 1024 * 1024 )) == NULL ) {
	syslog( LOG_ERR, "snet_attach: %m" );
	exit( 1 );
    }
    remote_addr = strdup( inet_ntoa( sin->sin_addr ));

    if (( hp = gethostbyaddr( (char *)&sin->sin_addr,
	    sizeof( struct in_addr ), AF_INET )) == NULL ) {
	remote_host = strdup( remote_addr );
    } else {
	/* set global remote_host for retr command */
	remote_host = strdup( hp->h_name );
	for ( p = remote_host; *p != '\0'; p++ ) {
	    *p = tolower( *p );
	}
    }

    syslog( LOG_INFO, "child for [%s] %s",
	    inet_ntoa( sin->sin_addr ), remote_host );

    if ( setsockopt( fd, 6, TCP_NODELAY, &one, sizeof( one )) < 0 ) {
	syslog( LOG_ERR, "setsockopt: %m" );
    }

    if ( maxconnections != 0 ) {
	if ( connections > maxconnections ) {
	    syslog( LOG_INFO, "%s: connection refused: server busy\r\n",
		    remote_host );
	    snet_writef( sn, "%d Server busy\r\n", 420 );
	    exit( 1 );
	}
    }

    if (( access_list = list_new( )) == NULL ) {
	syslog( LOG_ERR, "new_list: %m" );
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	return( -1 );
    }
    
    if ( authlevel == 0 ) {
	/* lookup proper command file based on the hostname, IP or CN */
	if ( command_k( "config", 0 ) < 0 ) {
	    syslog( LOG_INFO, "%s: Access denied: Not in config file",
		remote_host );
	    snet_writef( sn, "%d No access for %s\r\n", 500, remote_host );
	    exit( 1 );
	} else {
	    if ( read_kfile( sn, command_file ) != 0 ) {
		/* error message given in read_kfile */
		exit( 1 );
	    }
	    commands = auth;
	    ncommands = sizeof( auth ) / sizeof( auth[ 0 ] );
	}
    }

    if ( gethostname( hostname, MAXHOSTNAMELEN ) < 0 ) {
	syslog( LOG_ERR, "gethostname: %m" );
	exit( 1 );
    }

    snet_writef( sn, "200%sRAP 1 %s %s radmind access protocol\r\n",
	rap_extensions ? "-" : " ", hostname, version );
    if ( rap_extensions ) {
	snet_writef( sn, "200 CAPA" ); 
#ifdef HAVE_ZLIB
	if ( max_zlib_level > 0 ) {
	    snet_writef( sn, " ZLIB" ); 
	}
#endif /* HAVE_ZLIB */
	snet_writef( sn, " REPO" ); 
	snet_writef( sn, "\r\n" ); 
    }

    /*
     * 60 minutes
     * To make fsdiff | lapply work, when fsdiff will take a long time,
     * we allow the server to wait a long time.
     */
    tv.tv_sec = 60 * 60;
    tv.tv_usec = 0 ;
    while (( line = snet_getline( sn, &tv )) != NULL ) {
	tv.tv_sec = 60 * 60;
	tv.tv_usec = 0; 

	if ( debug ) {
	    fprintf( stderr, "<<< %s\n", line );
	}

	if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "argcargv: %m" );
	    return( 1 );
	}

	if ( ac == 0 ) {
	    snet_writef( sn, "%d Illegal null command\r\n", 501 );
	    continue;
	}

	for ( i = 0; i < ncommands; i++ ) {
	    n = MAX( strlen( av[ 0 ] ), 4 );
	    if ( strncasecmp( av[ 0 ], commands[ i ].c_name, n ) == 0 ) {
		break;
	    }
	}
	if ( i >= ncommands ) {
	    snet_writef( sn, "%d Command %s unrecognized\r\n", 500, av[ 0 ] );
	    continue;
	}
	if ( (*(commands[ i ].c_func))( sn, ac, av ) < 0 ) {
	    break;
	}

    }

    snet_writef( sn, "%d Server closing connection\r\n", 444 );

    if ( line == NULL ) {
	syslog( LOG_ERR, "snet_getline: %m" );
    }
    return( 0 );
}
