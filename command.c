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

#define	DEFAULT_MODE 0444
#define DEFAULT_UID     0
#define DEFAULT_GID     0

#define COMMAND 0
#define TRANSCRIPT 1
#define SPECIAL 2
#define FILE 3


int		f_quit ___P(( SNET *, int, char *[] ));
int		f_noop ___P(( SNET *, int, char *[] ));
int		f_help ___P(( SNET *, int, char *[] ));
int		f_stat ___P(( SNET *, int, char *[] ));
int		f_retr ___P(( SNET *, int, char *[] ));
int		f_stor ___P(( SNET *, int, char *[] ));

char		command_file[ MAXPATHLEN ];

    int
f_quit( sn, ac, av )
    SNET		*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d QUIT OK, closing connection\r\n", 201 );
    exit( 0 );
}

    int
f_noop( sn, ac, av )
    SNET		*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d NOOP OK\r\n", 202 );
    return( 0 );
}

    int
f_help( sn, ac, av )
    SNET		*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d What is this, SMTP?\r\n", 203 );
    return( 0 );
}

    int
f_retr( sn, ac, av )
    SNET		*sn;
    int		ac;
    char	*av[];
{

    unsigned int	readlen;
    struct stat		st;
    struct timeval	tv;
    char		buf[8192];
    char		path[ MAXPATHLEN ];
    char		*tmp, *d_path, *d_xscript, d_pth[ MAXPATHLEN ];
    int			fd;

    /* XXX check auth */

    if ( ac < 2 ) { 
	snet_writef( sn, "%d RETR Syntax error\r\n", 540 );
	return( 1 );
    }

    if ( strcasecmp( av[ 1 ], "COMMAND" ) == 0 ) {
	if ( ac != 2 ) { 
	    snet_writef( sn, "%d RETR Syntax error\r\n", 540 );
	    return( 1 );
	}
        sprintf( path, "%s", command_file );

    } else if ( strcasecmp( av[ 1 ], "TRANSCRIPT" ) == 0 ) {
        if ( ac != 3 ) {
	    return( 1 );
	}
        
	d_path = decode( av[ 2 ] );
	if ( strlen( av[ 1 ] ) + strlen(  av[ 2 ] ) + 1 > MAXPATHLEN ) {
	    syslog( LOG_WARNING, 
		"Overflow attempt: %s/%s longer than MAXPATHLEN", 
		    av[ 1 ], av[ 2 ] );
	    return( -1 );
	}
	    
        sprintf( path, "transcript/%s", d_path );

    } else if ( strcasecmp( av[ 1 ], "SPECIAL" ) == 0 ) {

	if ( ac != 3 ) {
	    snet_writef( sn, "%d RETR Syntax error\r\n", 540 );
	    return( 1 );
	}
	d_path = decode( av[ 2 ] );
        
	if ( strlen( av[ 1 ] ) + strlen( av[ 2 ] ) + 1 > MAXPATHLEN ) {
	    syslog( LOG_WARNING, 
		"Overflow attempt: special/%s longer than MAXPATHLEN", d_path );
	    return( -1 );
	}
	    
        sprintf( path, "special/%s", d_path );
    } else if ( strcasecmp( av[ 1 ], "FILE" ) == 0 ) {
        if ( ac != 4 ) {
	    snet_writef( sn, "%d RETR Syntax error\r\n", 540 );
	    return( 1 );
	}

	/* use local mem, because decode reuses memory */
	tmp = decode( av[ 3 ] );
	memcpy( d_pth, tmp, MAXPATHLEN );
	d_path = d_pth;
	d_xscript = decode( av[ 2 ] );

	if ( strlen( av[ 1 ] ) + strlen( av[ 2 ] ) + 
		strlen( d_path ) + 2 > MAXPATHLEN ) {
	    syslog( LOG_WARNING, 
		"Overflow attempt: %s/%s/%s longer than MAXPATHLEN", 
		    av[ 1 ], d_xscript, d_path );
	    return( 1 );
	}
	sprintf( path, "file/%s/%s", d_xscript, d_path );
    } else {
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


/* lookup the appropriate command file for the connected host */

    int
cmd_lookup( path_config )
    char	*path_config;
{
 
    SNET		*sn;
    char	**av, *line;
    int		ac;

    if (( sn = snet_open( path_config, O_RDONLY, 0, 0 )) == NULL ) {
        syslog( LOG_ERR, "cmd_lookup: snet_open: %s", path_config );
	return( -1 );
    }

    while (( line = snet_getline( sn, NULL )) != NULL ) {
        if (( ac = argcargv( line, &av )) < 0 ) {
	    perror( "argcargv" );
	    return( -1 );
	}

	if ( ( ac == 0 ) || ( *av[ 0 ] == '#' ) ) {
	    continue;
	}

	if ( strcasecmp( av[ 0 ], c_hostname ) == 0 ) {
	    sprintf( command_file, "command/%s", av[ 1 ] );
	    return( 0 );
	}
    }

    /* If we get here, the host that connected is not in the config
       file. So screw him. */

    syslog( LOG_INFO, "host not in config file: %s", c_hostname );
    return( -1 );
}

/* Pass this function the ENCODED version of the file... */
    char **
find_file( transcript, file )
    char 	*transcript, *file;
{

    SNET		*sn;
    int		ac;
    char	*line, **av;

    if (( sn = snet_open( transcript, O_RDONLY, 0, 0 )) == NULL ) {
	return( NULL );
    }

    while (( line = snet_getline( sn, NULL )) != NULL ) {
	if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "find_file: %s", transcript );
	    return( NULL );
	}

	if ( *av[ 0 ] != 'f' ) {
	    continue;
	}

	if ( strcmp( av[ 1 ], file ) == 0 ) { 
	    if ( snet_close( sn ) < 0 ) {
	        syslog( LOG_ERR, "find_file: snet_close: %m" );
	    }
	    return( av );
	}
    }

    return( NULL );
}

    int
f_stat( sn, ac, av )
    SNET		*sn;
    int		ac;
    char	*av[];
{

    char 		path[ MAXPATHLEN ];
    char		chksum_b64[ 29 ];
    struct stat		st;
    int			req_type;
    char		*d_path, *enc_file;

    if ( ac < 2 ) {
	snet_writef( sn, "%d STAT Syntax error\r\n", 530 );
	return( 1 );
    }
        
    if ( strcasecmp( av[ 1 ], "command" ) == 0 ) {
	req_type = COMMAND;
        sprintf( path, "%s", command_file );
    } else if ( strcasecmp( av[ 1 ], "transcript" ) == 0 ) {
        req_type = TRANSCRIPT;
        if ( ac != 3 ) {
	    snet_writef( sn, "%d STAT Syntax error\r\n", 530 );
	    return( 1 );
	}

	d_path = decode( av[ 2 ] );
	/* XXX can transcripts have '/'s */
	if ( strchr( d_path, '/' ) != NULL ) {
	    snet_writef(sn, "%d Illegal name, %s\r\n", 532, av[ 2 ]);
	    return( 1 );
	}

	if ( strlen( av[ 1 ] ) + strlen( d_path ) + 2 > MAXPATHLEN ) {
	    syslog( LOG_WARNING, 
	    "Overflow attempt: %s/%s longer than MAXPATHLEN", av[ 1 ], d_path );
	    return( 1 );
	}

	sprintf( path, "%s/%s", "transcript", d_path );
        
    } else if ( strcasecmp( av[ 1 ], "special" ) == 0 ) {
        req_type = SPECIAL;
        if ( ac != 3 ) {
	    snet_writef( sn, "%d STAT Syntax error\r\n", 530 );
	    return( 1 );
	}

	d_path = decode( av[ 2 ] );

	if ( strstr( d_path, "../" ) != NULL ) {
	    syslog( LOG_WARNING, "leet d00d at the gates!: %s", av[ 2 ] );
	    return( 1 );
	}
        
	if (( strlen( av[ 1 ] ) + strlen( d_path ) + strlen( c_hostname ) + 3 )
							> MAXPATHLEN ) {
	    syslog( LOG_WARNING, 
		"Overflow attempt: %s/%s-%s longer than MAXPATHLEN", 
		av[ 1 ], d_path, c_hostname );
	    return( -1 );
	}
	sprintf( path, "%s/%s-%s", "special", d_path, c_hostname );

    } else {
	snet_writef( sn, "%d STAT Syntax error\r\n", 530 );
	return( 1 );
    }
        

    if ( stat( path, &st ) < 0 ) {
        syslog( LOG_ERR, "f_stat: stat: %m" );
	snet_writef( sn, "%d Access Error: %s\r\n", 531, path );
	return( 1 );
    }

    /* chksums here */
    do_chksum( path, chksum_b64 );

    snet_writef( sn, "%d Returning STAT information\r\n", 230 );
    switch ( req_type ) {
    case COMMAND:
	snet_writef( sn, "%s %s %o %d %d %d %d %s\r\n", "f", "command", 
		    DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, st.st_mtime, 
		    st.st_size, chksum_b64 );
	return( 0 );
        
		    
    case TRANSCRIPT:
	snet_writef( sn, "%s %s %o %d %d %d %d %s\r\n", "f", av[ 2 ], 
		    DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, st.st_mtime, 
		    st.st_size, chksum_b64 );
	return( 0 );
    
    case SPECIAL:
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
	 * (from find_file, and that will blow away the current values
	 * for av[ 2 ]
	 */
	if ( ( enc_file = (char *)malloc( strlen( av[ 2 ] + 1 ) ) ) == NULL ) {
	    syslog( LOG_ERR, "f_stat: malloc: %m" );
	    return( -1 );
	}
	strcpy( enc_file, av[ 2 ] );

	if ( ( av = find_file( path, av[ 2 ] ) ) == NULL ) {
	    /* check global xscripts file */
	    if ( ( av = find_file( _PATH_TRANSCRIPTS, enc_file ) ) == NULL ) {
		snet_writef( sn, "%s %s %o %d %d %d %d %d\r\n", "f", enc_file, 
		    DEFAULT_MODE, DEFAULT_UID, DEFAULT_GID, 
		    st.st_mtime, st.st_size, chksum_b64 );
		return( 0 );
	    }
	}

	snet_writef( sn, "%s %s %s %s %s %d %d %d\r\n", "f", enc_file, 
	    av[ 3 ], av[ 4 ], st.st_mtime, st.st_size, chksum_b64 );

	return( 0 );

    default:
        return( 1 );
    }
}


    int 
create_directories( path ) 
    char	*path;
{

    char 	*i;
    int		done = 0;

    while ( *path == '/' ) {
        path++;
    }

    i = path;

    while ( 1 ) {
	if ( ( i = strchr( i, '/' ) ) == NULL ) {
	    break;
	}
    
	*i = '\0';
	printf("Attempting %s\n", path );
	/* make a dir */
	if ( mkdir( path, 0777 ) < 0 ) {
	    if ( errno != EEXIST ) {
		return( -1 );
	    }
	}
	*i = '/';
	if ( done ) {
	    break;
	}
        i++;
    }
    
    return( 0 );
}



char	upload_xscript[ MAXPATHLEN ];

    int
f_stor( sn, ac, av )
    SNET		*sn;
    int		ac;
    char	*av[];
{

    char 		*sizebuf;
    char		xscriptdir[ MAXPATHLEN ];
    char		upload_file[ MAXPATHLEN ];
    char		buff[ 8192 ];
    char		*upload, *line;
    char		*tmp;
    char		*d_path, *d_xscript, d_pth[ MAXPATHLEN ];
    int			size, fd;
    unsigned int	r;
    struct timeval	tv;
    int			remain;
 
    if ( ac < 2 ) {
	snet_writef( sn, "%d STOR Syntax error\r\n", 550 );
	return( 1 );
    }

    if ( strcasecmp( av[ 1 ], "command" ) == 0 ) {
        if ( ac != 2 ) { 
	    snet_writef( sn, "%d STOR Syntax error\r\n", 550 );
	    return( 1 );
	}

	/* pathlen check 
	 * 5 = tmp/ + null char
	 */
        if ( strlen( command_file + 5 ) > MAXPATHLEN )  {
	    syslog( LOG_WARNING, "tmp/%s longer than MAXPATHLEN", 
		    command_file );
	    return( 1 );
	}
	sprintf( upload_file, "tmp/%s", command_file );
	if (( fd = open( command_file, O_CREAT|O_EXCL|O_WRONLY, 0444 )) < 0 ) {
	    if ( errno == EEXIST ) {
		snet_writef( sn, "%d File Exists: %s\r\n", 555, command_file );
		return( 1 );
	    } else {
		syslog( LOG_ERR, "f_stor: open: %m" );
		return( -1 );
	    }
	}
	    
    } else if ( strcasecmp( av[ 1 ], "transcript" ) == 0 ) {
        if ( ac != 3 ) { 
	    snet_writef( sn, "%d STOR Syntax error\r\n", 550 );
	    return( 1 );
	}

	/* hot decoding action */

	d_path = decode( av[ 2 ] );
	/* Check for a / or a ../ in the xscript name */
	if ( strchr( d_path, '/' ) != NULL ) {
	    snet_writef( sn, "%d / Not Allowed\r\n", 554 );
	    return( 0 );
	}
	/* 15 = tmp/transcript + null char */

        if ( ( strlen( d_path ) + 15 ) > MAXPATHLEN ) {
	    syslog( LOG_WARNING, "tmp/transcript/%s longer than MAXPATHLEN", 
		    d_path );
	    return( 1 );
	}

        sprintf( xscriptdir, "tmp/file/%s", d_path );
        sprintf( upload_file, "tmp/transcript/%s", d_path );

	/* don't decode the transcript name, since it will just be
	 * used later to compare in a stor file.
	 */
	strcpy( upload_xscript, av[ 2 ] );

	/* make the directory for the files of this xscript to live in. */
	if ( mkdir ( xscriptdir, 0777 ) < 0 ) {
	    if ( errno == EEXIST ) {
	        snet_writef( sn, "%d Transcript exists\r\n", 551 );
		return( 0 );
	    }
	    syslog( LOG_ERR, "f_stor: mkdir: %s: %m", xscriptdir );
	    return( -1 );
	}

	if ( ( fd = open( upload_file, O_CREAT|O_EXCL|O_WRONLY, 0444 ) ) < 0 ) {
	    if ( errno == EEXIST ) {
		snet_writef( sn, "%d File Exists: %s\r\n", 555, upload_file );
		return( 1 );
	    } else {
		syslog( LOG_ERR, "f_stor: open: %m" );
		return( -1 );
	    }
	}

    } else if ( strcasecmp( av[ 1 ], "file" ) == 0 ) {
        if ( ac != 4 ) { 
	    snet_writef( sn, "%d STOR Syntax error\r\n", 550 );
	    return( 1 );
	}

	/* client must have provided a transcript name before giving 
	 * files in that transcript
	 *
	 * Is this really a good idea?  This means that if the client
	 * loses connection in the middle of a bunch of files in a large
	 * transcript, he has to start over, she can't even re-upload the
	 * transcript file, because stuff can't be overwritten...
	 * Should we consider allowing overwrites at least?
	 */
	if ( ( strcmp( upload_xscript, av[ 2 ] ) != 0 ) ) {
	    snet_writef( sn, "%d Incorrect Transcript\r\n", 552 );
	    return( 0 );
	}

	/* hot decoding action */
	/* This function uses the same memory, so use a temp. */
	tmp = decode( av[ 3 ] );
	memcpy( d_pth, tmp, MAXPATHLEN );
	d_path = d_pth;
	d_xscript = decode( av[ 2 ] );

	/* Check for a ../ in the xscript name */
	if ( strstr( d_path, "../" ) != NULL ) {
	    snet_writef( sn, "%d / Not Allowed\r\n", 554 );
	    return( 0 );
	}

	/* 9 = tmp/file + null char */
        if ( ( strlen( d_xscript ) + strlen( d_path ) + 9 ) > MAXPATHLEN ) {
	    syslog( LOG_WARNING, "tmp/file/%s/%s longer than MAXPATHLEN", 
		    d_xscript, d_path );
	    return( 1 );
	}

	sprintf( upload_file, "tmp/file/%s/%s", d_xscript, d_path );

	/* Create directories.  we can't do a
	 * "stor file rh7.0 bin/ls"
	 * unless the server creates tmp/rh7.0/bin first.
	 */

	if ( ( fd = open( upload_file, O_CREAT|O_EXCL|O_WRONLY, 0444 ) ) < 0 ) {
	    if ( errno == EEXIST ) {
		snet_writef( sn, "%d File Exists: %s\r\n", 555, upload );
		return( 1 );
	   } else if ( errno == ENOENT ) {
		if ( create_directories( upload_file ) < 0 ) {
		    if ( close( fd ) < 0 ) {
			syslog( LOG_ERR, "f_stor: close: %m" );
		    }
		    return( -1 );
		}
		if ( ( fd = open( upload_file, O_CREAT|O_EXCL|O_WRONLY, 0444 ))
									< 0 ) {
		    if ( errno == EEXIST ) {
			snet_writef( sn, "%d File Exists: %s\r\n", 555, upload);
			return( 1 );
		    } else {
			syslog( LOG_ERR, "f_stor: open: %m" );
			return( -1 );
		    }
		}

	    } else {
		syslog( LOG_ERR, "f_stor: open: %m" );
		return( -1 );
	    }
	}

    } else {
        snet_writef( sn, "%d STOR Syntax error\r\n", 550 );
	return( 0 );
    }

    /* Do the transfer*/
 
    snet_writef( sn, "%d Storing file\r\n", 350 );
    /* get the size */
    /* Will there be a limit? */

    tv.tv_sec = 10 * 60;
    tv.tv_usec = 0;
    
    if ( ( sizebuf = snet_getline( sn, &tv ) ) == NULL ) {
	syslog( LOG_ERR, "f_stor: snet_getline: %m" );
	return( -1 );
    }
    size = atoi( sizebuf );

    remain = size;
    while ( remain > 0 ) {
	if ( ( r = snet_read( sn, buff, (int)( remain < sizeof(buff) ? remain : 
			    sizeof( buff ) ), &tv ) ) < 0 ) {
	    syslog( LOG_ERR, "f_stor: snet_read: %m" );
	    return( -1 );
	}
	remain -= r;

	if ( write( fd, buff, r ) != r ) {
	    syslog( LOG_ERR, "f_stor: write: %m" );
	    return( -1 );
	}
    }

    tv.tv_sec = 10 * 60;
    tv.tv_usec = 0;
    if ( ( line = snet_getline( sn, &tv ) ) == NULL ) {
        syslog( LOG_ERR, "f_stor: snet_getline: %m" );
	return( -1 );
    } else {
	if ( strcmp( line, "." ) != 0 ) {
	    /* #$@% you, client, you lying sack of $@%#! */
	    if ( close( fd ) < 0 ) {
		syslog( LOG_ERR, "f_stor: close: %m" );
	    }
	    /* should I warn the client that he lied to me? */
	    snet_writef( sn, "%d File stored\r\n", 250 );
	    return( 1 );
	}
    }
    
    if ( close( fd ) < 0 ) {
        syslog( LOG_ERR, "f_stor: close: %m" );
    }

    snet_writef( sn, "%d File stored\r\n", 250 );
    return( 0 );
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
    SNET			*sn;
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

    if ( cmd_lookup( _PATH_CONFIG ) < 0 ) {
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
