/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
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

#ifdef sun
#define MIN(a,b)	((a)<(b)?(a):(b))
#define MAX(a,b)	((a)>(b)?(a):(b))
#endif /* sun */

#define	DEFAULT_MODE 0444
#define DEFAULT_UID     0
#define DEFAULT_GID     0

#define K_COMMAND 1
#define K_TRANSCRIPT 2
#define K_SPECIAL 3
#define K_FILE 4

int 		list_transcripts( SNET *sn );

int		f_quit ___P(( SNET *, int, char *[] ));
int		f_noop ___P(( SNET *, int, char *[] ));
int		f_help ___P(( SNET *, int, char *[] ));
int		f_stat ___P(( SNET *, int, char *[] ));
int		f_retr ___P(( SNET *, int, char *[] ));
int		f_stor ___P(( SNET *, int, char *[] ));
int		f_noauth ___P(( SNET *, int, char *[] ));
int		f_notls ___P(( SNET *, int, char *[] ));
int		f_starttls ___P(( SNET *, int, char *[] ));
#ifdef HAVE_LIBPAM
int		f_login ___P(( SNET *, int, char *[] ));
int 		exchange( int num_msg, struct pam_message **msgm,
		    struct pam_response **response, void *appdata_ptr );
#endif /* HAVE_LIBPAM */


char		*user = NULL;
char		*password = NULL;
char		*remote_host = NULL;
char		*remote_addr = NULL;
char		*remote_cn = NULL;
char		special_dir[ MAXPATHLEN ];
char		command_file[ MAXPATHLEN ];
char		upload_xscript[ MAXPATHLEN ];
const EVP_MD    *md = NULL;
struct list	*tran_list = NULL;
int		ncommands = 0;
int		authorized = 0;
char		hostname[ MAXHOSTNAMELEN ];

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
#ifdef HAVE_LIBPAM
    { "LOGIn",       	f_notls },
#endif /* HAVE_LIBPAM */
};

struct command	noauth[] = {
    { "QUIT",		f_quit },
    { "NOOP",		f_noop },
    { "HELP",		f_help },
    { "STATus",		f_noauth },
    { "RETRieve",	f_noauth },
    { "STORe",		f_noauth },
#ifdef HAVE_LIBPAM
    { "LOGIn",       	f_noauth },
#endif /* HAVE_LIBPAM */
};

struct command	auth[] = {
    { "QUIT",		f_quit },
    { "NOOP",		f_noop },
    { "HELP",		f_help },
    { "STATus",		f_stat },
    { "RETRieve",	f_retr },
    { "STORe",		f_stor },
    { "STARttls",       f_starttls },
#ifdef HAVE_LIBPAM
    { "LOGIn",       	f_login },
#endif /* HAVE_LIBPAM */
};

struct command *commands  = NULL;

    int
f_quit( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d QUIT OK, closing connection\r\n", 201 );
    exit( 0 );
}

    int
f_noop( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d NOOP OK\r\n", 202 );
    return( 0 );
}

    int
f_help( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d What is this, SMTP?\r\n", 203 );
    return( 0 );
}

    int
f_noauth( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d No access for %s\r\n", 500, remote_host );
    exit( 1 );
}

    int
f_notls( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
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
	if ( ac != 2 ) {
	    return( -1 );
	}

	if ( strlen( command_file + 5 ) > MAXPATHLEN )  {
	    syslog( LOG_WARNING, "[tmp]/%s longer than MAXPATHLEN",
		    command_file );
	    return( -1 );
	}
	return( K_COMMAND );
    }

    if ( strcasecmp( av[ 1 ], "SPECIAL" ) == 0 ) {
	if ( ac != 3 ) {
	    return( -1 );
	}

	if ( strlen( av[ 1 ] ) + strlen( av[ 2 ] ) +
		strlen( remote_host) + 5 > MAXPATHLEN ) {
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
f_retr( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
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
        sprintf( path, "%s", command_file );
	break;

    case K_TRANSCRIPT:
	if (( d_tran = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_retr: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	/* Check for access */
	if ( !list_check( tran_list, d_tran )) {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", d_tran );
	    snet_writef( sn, "%d No access for %s\r\n", 540, d_tran );
	    return( 1 );
	}

	sprintf( path, "transcript/%s", d_tran );
	break;

    case K_SPECIAL:
	if (( d_path = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_retr: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	sprintf( path, "%s/%s", special_dir, d_path );
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
	if ( list_check( tran_list, d_tran )) {
	    break;
	} else {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", d_tran );
	    snet_writef( sn, "%d No access for %s:%s\r\n", 540, d_tran,
		d_path );
	    return( 1 );
	}

	sprintf( path, "file/%s/%s", d_tran, d_path );
	free( d_path );
	break;

    default:
	snet_writef( sn, "%d RETR Syntax error\r\n", 540 );
	return( 1 );
    }

    if ( ( fd = open( path, O_RDONLY, 0 ) ) < 0 ) {
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

    snet_writef( sn, "%d Retrieving file\r\n", 240 );
    /*
     * Here's a problem.  Do we need to add long long support to
     * snet_writef?
     */
    snet_writef( sn, "%" PRIofft "d\r\n", st.st_size );

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
special_t( char *transcript, char *epath )
{
    FILE		*fs;
    int			ac, len;
    char		**av;
    static char		line[ MAXPATHLEN ];

    if (( fs = fopen( transcript, "r" )) == NULL ) {
	return( NULL );
    }

    while ( fgets( line, MAXPATHLEN, fs ) != NULL ) {
	len = strlen( line );
	if (( line[ len - 1 ] ) != '\n' ) {
	    syslog( LOG_ERR, "special_t: %s: line too long", transcript );
	    break;
	}

	if (( ac = argcargv( line, &av )) != 8 ) {
	    continue;
	}
	if (( *av[ 0 ] != 'f' ) && ( *av[ 0 ] != 'a' )) {
	    continue;
	}

	if ( strcmp( av[ 1 ], epath ) == 0 ) { 
	    (void)fclose( fs );
	    return( av );
	}
    }

    (void)fclose( fs );
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
        sprintf( path, "%s", command_file );
	break;

    case K_TRANSCRIPT:
	if (( d_tran = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_stat: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	/* Check for access */
	if ( !list_check( tran_list, d_tran )) {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", d_tran );
	    snet_writef( sn, "%d No access for %s\r\n", 540, d_tran );
	    return( 1 );
	}

	sprintf( path, "transcript/%s", d_tran );
	break;

    case K_SPECIAL:
	if (( d_path = decode( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_stat: decode: buffer too small" );
	    snet_writef( sn, "%d Line too long\r\n", 540 );
	    return( 1 );
	} 

	sprintf( path, "%s/%s", special_dir, d_path);
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
	fprintf( stderr, "%s: unsupported checksum\n", "sha1" );
	exit( 1 );
    }
    if ( do_cksum( path, cksum_b64 ) < 0 ) {
	snet_writef( sn, "%d Checksum Error: %s: %m\r\n", 500, path );
	syslog( LOG_ERR, "do_cksum: %s: %m", path );
	return( 1 );
    }

    snet_writef( sn, "%d Returning STAT information\r\n", 230 );
    switch ( key ) {
    case K_COMMAND:
	snet_writef( sn, "%s %s %o %d %d %d %" PRIofft "d %s\r\n",
		"f", "command", 
		DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID,
		st.st_mtime, st.st_size, cksum_b64 );
	return( 0 );
        
		    
    case K_TRANSCRIPT:
	snet_writef( sn, "%s %s %o %d %d %d %" PRIofft "d %s\r\n",
		"f", av[ 2 ], 
		DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID,
		st.st_mtime, st.st_size, cksum_b64 );
	return( 0 );
    
    case K_SPECIAL:
	/*  status on a special file comes from 1 of three cases:
	 *  1. A transcript in the special file directory
	 *  2. A transcript in the Transcript dir with .T appended
	 *  3. No transcript is found, and constants are returned
	 */

        /* look for transcript containing the information */
	if ( ( strlen( path ) + 2 ) > MAXPATHLEN ) {
	    syslog( LOG_WARNING, 
		"f_stat: transcript path longer than MAXPATHLEN" );

	    /* return constants */
	    snet_writef( sn, "%s %s %o %d %d %d %" PRIofft "d %s\r\n",
		    "f", av[ 2 ], 
		    DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID,
		    st.st_mtime, st.st_size, cksum_b64 );
	    return( 0 );
	}

	/* if allowable, check for transcript in the special file directory */

	strcat( path, ".T" );

	/* store value of av[ 2 ], because argcargv will be called
	 * from special_t(), and that will blow away the current values
	 * for av[ 2 ]
	 *
	 * Could just use new argvargc API... XXX  Notice how we never free
	 * env_file...
	 */

	if (( enc_file = strdup( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_stat: strdup: %s %m", av[ 2 ] );
	    return( -1 );
	}

	if (( av = special_t( path, enc_file )) == NULL ) {
	    if (( av = special_t( "transcript/special.T", enc_file ))
		    == NULL ) {
		snet_writef( sn, "%s %s %o %d %d %d %" PRIofft "d %s\r\n",
			"f", enc_file, 
			DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, 
			st.st_mtime, st.st_size, cksum_b64 );
		return( 0 );
	    }
	}
	snet_writef( sn, "%s %s %s %s %s %d %" PRIofft "d %s\r\n",
		av[ 0 ], enc_file,
		av[ 2 ], av[ 3 ], av[ 4 ],
		st.st_mtime, st.st_size, cksum_b64 );

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
    off_t		len;
    ssize_t		rc;
    struct timeval	tv;

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
        sprintf( xscriptdir, "tmp/file/%s", d_tran );
        sprintf( upload, "tmp/transcript/%s", d_tran );

	/* keep encoded transcript name, since it will just be
	 * used later to compare in a stor file.
	 */
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
	    sprintf( upload, "tmp/file/%s%s", d_tran, d_path );
	} else {
	    sprintf( upload, "tmp/file/%s/%s", d_tran, d_path );
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
f_starttls( snet, ac, av )
    SNET                        *snet;
    int                         ac;    
    char                        *av[];
{
    int                         rc;
    X509                        *peer;
    char                        buf[ 1024 ];

    if ( ac != 1 ) {  
        snet_writef( snet, "%d Syntax error (no parameters allowed)\r\n", 501 );
        return( 1 );
    } else {
	snet_writef( snet, "%d Ready to start TLS\r\n", 220 );
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
    /* At this point the client is also staring TLS */
    /* 1 is for server, 0 is client */
    if (( rc = snet_starttls( snet, ctx, 1 )) != 1 ) {
        syslog( LOG_ERR, "f_starttls: snet_starttls: %s",
                ERR_error_string( ERR_get_error(), NULL ) );
        snet_writef( snet, "%d SSL didn't work error! XXX\r\n", 501 );
        return( 1 );
    }

    if ( authlevel == 2 ) {
	if (( peer = SSL_get_peer_certificate( snet->sn_ssl ))
		== NULL ) {
	    syslog( LOG_ERR, "no peer certificate" );
	    return( -1 );
	}

	syslog( LOG_INFO, "CERT Subject: %s\n",
	    X509_NAME_oneline( X509_get_subject_name( peer ), buf,
	    sizeof( buf )));
	X509_free( peer );

	X509_NAME_get_text_by_NID( X509_get_subject_name( peer ),
	    NID_commonName, buf, sizeof( buf ));
	if (( remote_cn = strdup( buf )) == NULL ) {
	    syslog( LOG_ERR, "strdup: %m" );
	    return( -1 );
	}
	X509_free( peer );
    }

    /* get command file */
    if ( command_k( "config" ) < 0 ) {
	/* Client not in config */
	commands  = noauth;
	ncommands = sizeof( noauth ) / sizeof( noauth[ 0 ] );
    } else {
	/* Client in config */
	commands  = auth;
	ncommands = sizeof( auth ) / sizeof( auth[ 0 ] );

	if ( list_transcripts( snet ) != 0 ) {
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
f_login( snet, ac, av )
    SNET                        *snet;
    int                         ac;    
    char                        *av[];
{
    int				retval;
    pam_handle_t		*pamh;
    struct pam_conv		pam_conv = {
	exchange,
	NULL
    };

    if ( !checkuser ) {
	snet_writef( snet, "%d login not enabled\r\n", 502 );
	return( 1 );
    }
    /*
    if ( authlevel < 1 ) {
	snet_writef( snet, "%d login requires TLS\r\n", 503 );
	return( 1 );
    }
    */
    if ( ac != 3 ) {  
        snet_writef( snet, "%d Syntax error\r\n", 501 );
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
        snet_writef( snet, "%d Authentication Failed\r\n", 535 );
        syslog( LOG_ERR, "f_login: pam_start: %s\n",
	    pam_strerror( pamh, retval ));
	return( 1 );
    }

    /* is user really user? */
    if (( retval =  pam_authenticate( pamh, PAM_SILENT )) != PAM_SUCCESS ) {
        snet_writef( snet, "%d Authentication Failed\r\n", 535 );
        syslog( LOG_ERR, "f_login: pam_authenticate: %s\n",
	    pam_strerror( pamh, retval ));
	return( 1 );
    }
    free( password );

    /* permitted access? */
    if (( retval = pam_acct_mgmt( pamh, 0 )) != PAM_SUCCESS ) {
        snet_writef( snet, "%d Authentication Failed\r\n", 535 );
        syslog( LOG_ERR, "f_login: pam_acct_mgmt: %s\n",
	    pam_strerror( pamh, retval ));
	return( 1 );
    }

    if (( retval = pam_end( pamh, retval )) != PAM_SUCCESS ) {
        snet_writef( snet, "%d Authentication Failed\r\n", 535 );
        syslog( LOG_ERR, "f_login: pam_end: %s\n",
	    pam_strerror( pamh, retval ));
	return( 1 );
    }
    syslog( LOG_INFO, "%s: successfully logged in\n", user );
    snet_writef( snet, "%d %s successfully logged in\r\n", 205, user );
    authorized = 1;

    return( 0 );
}
#endif /* HAVE_LIBPAM */

/* sets command file for connected host */
    int
command_k( char *path_config )
{
    SNET	*sn;
    char	**av, *line, *p;
    char	temp[ MAXPATHLEN ];
    int		ac;
    int		linenum = 0;

    if (( sn = snet_open( path_config, O_RDONLY, 0, 0 )) == NULL ) {
        syslog( LOG_ERR, "command_k: snet_open: %s: %m", path_config );
	return( -1 );
    }

    while (( line = snet_getline( sn, NULL )) != NULL ) {
	linenum++;

        if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "argvargc: %m" );
	    return( -1 );
	}

	if ( ( ac == 0 ) || ( *av[ 0 ] == '#' ) ) {
	    continue;
	}
	if ( ac != 2 ) {
	    syslog( LOG_ERR, "config file: line %d: invalid number of \
		arguments\n", linenum );
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

	if (( remote_cn != NULL ) && wildcard( av[ 0 ], remote_cn )) {
	    sprintf( command_file, "command/%s", av[ 1 ] );
	    if ( snprintf( temp, MAXPATHLEN, "%s/%s", special_dir,
		    remote_cn ) >= MAXPATHLEN ) {
		syslog( LOG_ERR, "config file: line %d: special dir too long\n",
		    linenum );
		continue;
	    }
	    strcpy( special_dir, temp );
	    return( 0 );
	}
	if ( wildcard( av[ 0 ], remote_host )) {
	    sprintf( command_file, "command/%s", av[ 1 ] );
	    if ( snprintf( temp, MAXPATHLEN, "%s/%s", special_dir,
		    remote_host ) >= MAXPATHLEN ) {
		syslog( LOG_ERR, "config file: line %d: special dir too long\n",
		    linenum );
		continue;
	    }
	    strcpy( special_dir, temp );
	    return( 0 );
	} 
	if ( wildcard( av[ 0 ], remote_addr )) {
	    sprintf( command_file, "command/%s", av[ 1 ] );
	    if ( snprintf( temp, MAXPATHLEN, "%s/%s", special_dir,
		    remote_addr ) >= MAXPATHLEN ) {
		syslog( LOG_ERR, "config file: line %d: special dir too long\n",
		    linenum );
		continue;
	    }
	    strcpy( special_dir, temp );
	    return( 0 );
	} 
    }

    /* If we get here, the host that connected is not in the config
       file. So screw him. */

    syslog( LOG_ERR, "host not in config file: %s", remote_host );
    return( -1 );
}

    int
list_transcripts( SNET *sn )
{
    int			ac, linenum;
    char		kline[ MAXPATHLEN * 2 ];
    FILE		*f;
    char                **av;

    if (( tran_list = list_new( )) == NULL ) {
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	syslog( LOG_ERR, "new_list: %m" );
	return( -1 );
    }

    /* Create list of transcripts */
    if (( f = fopen( command_file, "r" )) == NULL ) {
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	syslog( LOG_ERR, "fopen: %s: %m", command_file );
	return( -1 );
    }
    linenum = 0;
    while ( fgets( kline, MAXPATHLEN, f ) != NULL ) {
	linenum++;
	ac = acav_parse( NULL, kline, &av );
	if (( ac == 0 ) || ( *av[ 0 ] == '#' )
		|| ( *av[ 0 ] == 's')) {
	    continue;
	}
	if ( ac != 2 ) {
	    snet_writef( sn,
		"%d Service not available, closing transmission channel\r\n",
		421 );
	    syslog( LOG_ERR, "%s: %d: invalid command line\n",
		    command_file, linenum );
	    return( -1 );
	}
	if ( list_insert( tran_list, av[ 1 ] ) != 0 ) {
	    snet_writef( sn,
		"%d Service not available, closing transmission channel\r\n",
		421 );
	    syslog( LOG_ERR, "list_insert: %m" );
	    return( -1 );
	}

    }
    if ( ferror( f )) {
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	syslog( LOG_ERR, "fgets: %m" );
	return( -1 );
    }
    if ( fclose( f ) < 0 ) {
	snet_writef( sn,
	    "%d Service not available, closing transmission channel\r\n", 421 );
	syslog( LOG_ERR, "fclose: %m" );
	return( -1 );
    }
    return( 0 );
}

    int
cmdloop( int fd, struct sockaddr_in *sin )
{
    SNET		*sn;
    struct hostent	*hp;
    char		*p;
    int			ac, i;
    unsigned int	n;
    char		**av, *line;
    struct timeval	tv;
    extern char		*version;
    extern int		connections;
    extern int		maxconnections;


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

    if ( maxconnections != 0 ) {
	if ( connections > maxconnections ) {
	    snet_writef( sn, "%d Server busy\r\n", 420 );
	    syslog( LOG_INFO, "%s: connection refused: server busy\r\n",
		    remote_host );
	    exit( 1 );
	}
    }
    
    if ( authlevel == 0 ) {
	/* lookup proper command file based on the hostname, IP or CN */
	if ( command_k( "config" ) < 0 ) {
	    snet_writef( sn, "%d No access for %s\r\n", 500, remote_host );
	    syslog( LOG_INFO, "%s: Access denied: Not in config file",
		    remote_host );
	    exit( 1 );
	} else {
	    if ( list_transcripts( sn ) != 0 ) {
		/* error message given in list_transcripts */
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

    snet_writef( sn, "%d RAP 1 %s %s radmind access protocol\r\n", 200,
	    hostname, version );

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
	    snet_writef( sn, "%d Command %s unregcognized\r\n", 500, av[ 0 ] );
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
