/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>

#include <snet.h>

#include "argcargv.h"
#include "command.h"
#include "auth.h"
#include "code.h"
#include "chksum.h"
#include "mkdirs.h"

#define	DEFAULT_MODE 0444
#define DEFAULT_UID     0
#define DEFAULT_GID     0

#define K_COMMAND 1
#define K_TRANSCRIPT 2
#define K_SPECIAL 3
#define K_FILE 4


int		f_quit ___P(( SNET *, int, char *[] ));
int		f_noop ___P(( SNET *, int, char *[] ));
int		f_help ___P(( SNET *, int, char *[] ));
int		f_stat ___P(( SNET *, int, char *[] ));
int		f_retr ___P(( SNET *, int, char *[] ));
int		f_stor ___P(( SNET *, int, char *[] ));

char		command_file[ MAXPATHLEN ];
char		upload_xscript[ MAXPATHLEN ];

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

	if ( strstr( av[ 3 ], "../" ) != NULL ) {
	    syslog( LOG_WARNING | LOG_AUTH, "attempt to access: %s", av[ 3 ] );
	    return( -1 );
	}

	if ( strlen( av[ 1 ] ) + strlen( av[ 2 ] ) +
		strlen( av[ 3 ] ) + 5 > MAXPATHLEN ) {
	    syslog( LOG_WARNING, "Overflow attempt: %s/%s/%s longer than
		    MAXPATHLEN", av[ 1 ], av[ 2 ], av[ 3 ] );
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

    unsigned int	readlen;
    struct stat		st;
    struct timeval	tv;
    char		buf[8192];
    char		path[ MAXPATHLEN ];
    char		*d_path;
    int			fd;

    switch ( keyword( ac, av )) {
    case K_COMMAND:
        sprintf( path, "%s", command_file );
	break;

    case K_TRANSCRIPT:
	sprintf( path, "transcript/%s", decode( av[ 2 ] ));
	break;

    case K_SPECIAL:
	sprintf( path, "special/%s/%s", remote_host, decode( av[ 2 ] ));
	break;

    case K_FILE:
	if (( d_path = strdup( decode( av[ 3 ] ))) == NULL ) {
	    syslog( LOG_ERR, "f_retr: strdup: %s: %m", av[ 3 ] );
	    snet_writef( sn, "%d Can't allocate memory: %s\r\n", 555, av[ 3 ] );
	    return( -1 );
	}
	sprintf( path, "file/%s/%s", decode( av[ 2 ] ), d_path );
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
    snet_writef( sn, "%d\r\n", st.st_size );

    /* dump file */

    while (( readlen = read( fd, buf, sizeof( buf ))) > 0 ) {
	tv.tv_sec = 10 * 60;
	tv.tv_usec = 0;
	if ( snet_write( sn, buf, (int)readlen, &tv ) != readlen ) {
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
	if ( *av[ 0 ] != 'f' ) {
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
    char		chksum_b64[ 29 ];
    struct stat		st;
    int			key;
    char		*enc_file, stranpath[ MAXPATHLEN ];

    switch ( key = keyword( ac, av )) {
    case K_COMMAND:
        sprintf( path, "%s", command_file );
	break;

    case K_TRANSCRIPT:
	sprintf( path, "transcript/%s", decode( av[ 2 ] ));
	break;

    case K_SPECIAL:
	sprintf( path, "special/%s/%s", remote_host, decode( av[ 2 ] ));
	break;

    default:
	snet_writef( sn, "%d STAT Syntax error\r\n", 530 );
	return( 1 );
    }
        

    if ( stat( path, &st ) < 0 ) {
        syslog( LOG_ERR, "f_stat: stat: %m" );
	snet_writef( sn, "%d Access Error: %s\r\n", 531, path );
	return( 1 );
    }

    /* chksums here */
    if ( do_chksum( path, chksum_b64 ) < 0 ) {
	snet_writef( sn, "%d Checksum Error: %s: %m\r\n", 500, path );
	syslog( LOG_ERR, "do_chksum: %s: %m", path );
	return( 1 );
    }

    snet_writef( sn, "%d Returning STAT information\r\n", 230 );
    switch ( key ) {
    case K_COMMAND:
	snet_writef( sn, "%s %s %o %d %d %d %d %s\r\n", "f", "command", 
		    DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, st.st_mtime, 
		    st.st_size, chksum_b64 );
	return( 0 );
        
		    
    case K_TRANSCRIPT:
	snet_writef( sn, "%s %s %o %d %d %d %d %s\r\n", "f", av[ 2 ], 
		    DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, st.st_mtime, 
		    st.st_size, chksum_b64 );
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
	    snet_writef( sn, "%s %s %o %d %d %d %d %s\r\n", "f", av[ 2 ], 
		DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, 
		st.st_mtime, st.st_size, chksum_b64 );
	    return( 0 );
	}

	/* if allowable, check for transcript in the special file directory */

	strcat( path, ".T" );

	/* store value of av[ 2 ], because argcargv will be called
	 * from special_t(), and that will blow away the current values
	 * for av[ 2 ]
	 */


	if (( enc_file = strdup( av[ 2 ] )) == NULL ) {
	    syslog( LOG_ERR, "f_stat: strdup: %s %m", av[ 2 ] );
	    snet_writef( sn, "%d Can't allocate memory: %s\r\n", 555, av[ 2 ]);
	    return( -1 );
	}

	if (( av = special_t( path, enc_file )) == NULL ) {
	    sprintf( stranpath, "%s/special.T", _PATH_TRANSCRIPTS );
	    if (( av = special_t( stranpath, enc_file )) == NULL ) {
		snet_writef( sn, "%s %s %o %d %d %d %d %s\r\n", "f", enc_file, 
		    DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, 
		    st.st_mtime, st.st_size, chksum_b64 );
		return( 0 );
	    }
	}
	snet_writef( sn, "%s %s %s %s %s %d %d %s\r\n", "f", enc_file, av[ 2 ],
		av[ 3 ], av[ 4 ], st.st_mtime, st.st_size, chksum_b64 );

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
    char		*d_path;
    int			fd;
    unsigned int	len, rc;
    struct timeval	tv;

    switch ( keyword( ac, av )) {

    case K_TRANSCRIPT:
        sprintf( xscriptdir, "tmp/file/%s", decode( av[ 2 ] ));
        sprintf( upload, "tmp/transcript/%s", decode( av[ 2 ] ));

	/* don't decode the transcript name, since it will just be
	 * used later to compare in a stor file.
	 */
	strcpy( upload_xscript, av[ 2 ] );

	/* make the directory for the files of this xscript to live in. */
	if ( mkdir ( xscriptdir, 0777 ) < 0 ) {
	    if ( errno == EEXIST ) {
	        snet_writef( sn, "%d Transcript exists\r\n", 551 );
		return( 1 );
	    }
	    snet_writef( sn, "%d %s: %s\r\n",
		    551, xscriptdir, strerror( errno ));
	    return( 1 );
	}
	break;

    case K_FILE:
	/* client must have provided a transcript name before giving 
	 * files in that transcript
	 */
	if (( strcmp( upload_xscript, av[ 2 ] ) != 0 )) {
	    snet_writef( sn, "%d Incorrect Transcript\r\n", 552 );
	    return( 1 );
	}

	/* decode() uses static mem, so strdup() */
	if (( d_path = strdup( decode( av[ 3 ] ))) == NULL ) {
	    syslog( LOG_ERR, "f_stor: strdup: %s: %m", av[ 3 ] );
	    snet_writef( sn, "%d Can't allocate memory: %s\r\n", 555, av [ 3]);
	    return( -1 );
	}
	sprintf( upload, "tmp/file/%s/%s", decode( av[ 2 ] ), d_path );
	free( d_path );
	break;

    default:
        snet_writef( sn, "%d STOR Syntax error\r\n", 550 );
	return( 1 ); 
    }


    if (( fd = open( upload, O_CREAT|O_EXCL|O_WRONLY, 0444 )) < 0 ) {
	if ( mkdirs( upload ) < 0 ) {
	    snet_writef( sn, "%d %s: %s\r\n", 555, upload, strerror( errno ));
	    return( 1 );
	}
	if (( fd = open( upload, O_CREAT|O_EXCL|O_WRONLY, 0444 )) < 0 ) {
	    snet_writef( sn, "%d %s: %s\r\n", 555, upload, strerror( errno ));
	    return( 1 );
	}
    }


    snet_writef( sn, "%d Storing file\r\n", 350 );

    tv.tv_sec = 10 * 60;
    tv.tv_usec = 0;
    if ( ( sizebuf = snet_getline( sn, &tv ) ) == NULL ) {
	syslog( LOG_ERR, "f_stor: snet_getline: %m" );
	return( -1 );
    }
    /* Will there be a limit? */
    len = atoi( sizebuf );

    for ( ; len > 0; len -= rc ) {
#define MIN(a,b)	((a)<(b)?(a):(b))
	tv.tv_sec = 10 * 60;
	tv.tv_usec = 0;
	if (( rc = snet_read(
		sn, buf, (int)MIN( len, sizeof( buf )), &tv )) <= 0 ) {
	    syslog( LOG_ERR, "f_stor: snet_read: %m" );
	    return( -1 );
	}

	if ( write( fd, buf, rc ) != rc ) {
	    snet_writef( sn, "%d %s: %s\r\n", 555, upload, strerror( errno ));
	    return( 1 );
	}
    }

    if ( close( fd ) < 0 ) {
	snet_writef( sn, "%d %s: %s\r\n", 555, upload, strerror( errno ));
	return( 1 );
    }

    tv.tv_sec = 10 * 60;
    tv.tv_usec = 0;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
        syslog( LOG_ERR, "f_stor: snet_getline: %m" );
	return( -1 );
    }

    /* make sure client agrees we're at the end */
    if ( strcmp( line, "." ) != 0 ) {
	snet_writef( sn, "%d Length doesn't match sent data\r\n", 555 );
	(void)unlink( upload );

	tv.tv_sec = 10 * 60;
	tv.tv_usec = 0;
	for (;;) {
	    if (( line = snet_getline( sn, &tv )) == NULL ) {
		syslog( LOG_ERR, "f_stor: snet_getline: %m" );
		return( -1 );
	    }
	    if ( strcmp( line, "." ) == 0 ) {
		break;
	    }
	}

	return( -1 );
    }

    snet_writef( sn, "%d File stored\r\n", 250 );
    return( 0 );
}

/* sets command file for connected host */
    int
command_k( path_config )
    char	*path_config;
{
    SNET	*sn;
    char	**av, *line;
    int		ac;

    if (( sn = snet_open( path_config, O_RDONLY, 0, 0 )) == NULL ) {
        syslog( LOG_ERR, "command_k: snet_open: %s: %m", path_config );
	return( -1 );
    }

    while (( line = snet_getline( sn, NULL )) != NULL ) {
        if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "argvargc: %m" );
	    return( -1 );
	}

	if ( ( ac == 0 ) || ( *av[ 0 ] == '#' ) ) {
	    continue;
	}

	if ( strcasecmp( av[ 0 ], remote_host ) == 0 ) {
	    sprintf( command_file, "command/%s", av[ 1 ] );
	    return( 0 );
	}
    }

    /* If we get here, the host that connected is not in the config
       file. So screw him. */

    syslog( LOG_INFO, "host not in config file: %s", remote_host );
    return( -1 );
}

struct command	commands[] = {
    { "QUIT",		f_quit },
    { "NOOP",		f_noop },
    { "HELP",		f_help },
    { "AUTHenticate",	f_auth },
    { "STATus",		f_stat },
    { "RETRieve",	f_retr },
    { "STORe",		f_stor },
};

int		ncommands = sizeof( commands ) / sizeof( commands[ 0 ] );
char		hostname[ MAXHOSTNAMELEN ];

    int
cmdloop( fd )
    int		fd;
{
    SNET		*sn;
    int			ac, i;
    unsigned int	n;
    char		**av, *line;
    struct timeval	tv;
    extern char		*version;

    if (( sn = snet_attach( fd, 1024 * 1024 )) == NULL ) {
	syslog( LOG_ERR, "snet_attach: %m" );
	exit( 1 );
    }
    
    /* lookup proper command file based on the hostname */

    if ( command_k( _PATH_CONFIG ) < 0 ) {
        snet_writef( sn, "%d Access Denied\r\n", 500 );
	exit( 1 );
    }

    if ( gethostname( hostname, MAXHOSTNAMELEN ) < 0 ) {
	syslog( LOG_ERR, "gethostname: %m" );
	exit( 1 );
    }

    snet_writef( sn, "%d RAP 1 %s %s radmind access protocol\r\n", 200,
	    hostname, version );

    tv.tv_sec = 10 * 60;	/* 10 minutes */
    tv.tv_usec = 0 ;
    while (( line = snet_getline( sn, &tv )) != NULL ) {
	tv.tv_sec = 10 * 60;
	tv.tv_usec = 0; 
	if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "argcargv: %m" );
	    return( 1 );
	}

	if ( ac ) {
	    for ( i = 0; i < ncommands; i++ ) {
#define MAX(a,b)	((a)>(b)?(a):(b))
		n = MAX( strlen( av[ 0 ] ), 4 );
		if ( strncasecmp( av[ 0 ], commands[ i ].c_name, n ) == 0 ) {
		    if ( (*(commands[ i ].c_func))( sn, ac, av ) < 0 ) {
			return( 1 );
		    }
		    break;
		}
	    }
	    if ( i >= ncommands ) {
		snet_writef( sn, "%d Command %s unregcognized\r\n", 500,
			av[ 0 ] );
	    }
	} else {
	    snet_writef( sn, "%d Illegal null command\r\n", 501 );
	}
    }

    return( 0 );
}
