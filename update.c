/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/stat.h>
#ifdef sun
#include <sys/mkdev.h>
#endif sun
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>

#include "update.h"
#include "code.h"

extern int quiet;
extern int linenum;

    int
update( const char *path, char *displaypath, int present, int newfile,
    struct stat *st, int tac, char **targv )
{
    mode_t              mode;
    struct utimbuf      times;
    uid_t               uid;
    gid_t               gid;
    dev_t               dev;


    switch ( *targv[ 0 ] ) {
    case 'a':
    case 'f':
	if ( tac != 8 ) {
	    fprintf( stderr, "%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}
	mode = strtol( targv[ 2 ], (char **)NULL, 8 );
	uid = atoi( targv[ 3 ] );
	gid = atoi( targv[ 4 ] );
	times.modtime = atoi( targv[ 5 ] );
	if ( !quiet ) {
	    if ( newfile ) {
		printf( "%s: created updating", displaypath );
	    } else {
		printf( "%s: updating", displaypath );
	    }
	}
	if( mode != st->st_mode ) {
	    if ( chmod( path, mode ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( !quiet ) printf( " mode" );
	}
	if( uid != st->st_uid  || gid != st->st_gid ) {
	    if ( chown( path, uid, gid ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( uid != st->st_uid ) {
		if ( !quiet ) printf( " uid" );
	    }
	    if ( gid != st->st_gid ) {
		if ( !quiet ) printf( " gid" );
	    }
	}

	if( times.modtime != st->st_mtime ) {
	    
	    times.actime = st->st_atime;
	    if ( utime( path, &times ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( !quiet ) printf( " time" ); 
	}
	break;

    case 'd':

	if ( tac != 5 ) {
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}

	mode = strtol( targv[ 2 ], (char **)NULL, 8 );
	uid = atoi( targv[ 3 ] );
	gid = atoi( targv[ 4 ] );

	if ( !present ) {
	    mode = strtol( targv[ 2 ], (char**)NULL, 8 );
	    if ( mkdir( path, mode ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    newfile = 1;
	    if ( lstat( path, st ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 1;
	}

	/* check mode */
	if ( !quiet ) {
	    if ( newfile ) {
		printf( "%s: created updating", displaypath );
	    } else {
		printf( "%s: updating", displaypath );
	    }
	}
	if( mode != st->st_mode ) {
	    if ( chmod( path, mode ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( !quiet ) printf( " mode" );
	}

	/* check uid & gid */
	if( uid != st->st_uid  || gid != st->st_gid ) {
	    if ( chown( path, uid, gid ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( uid != st->st_uid ) {
		if ( !quiet ) printf( " uid" );
	    }
	    if ( gid != st->st_gid ) {
		if ( !quiet ) printf( " gid" );
	    }
	}
	break;

    case 'h':
	if ( tac != 3 ) {
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}
	if ( link( decode( targv[ 2 ] ), path ) != 0 ) {
	    perror( path );
	    return( 1 );
	}
	if ( !quiet ) printf( "%s: hard linked to %s",
	    displaypath, decode( targv[ 2 ] ));
	break;

    case 'l':
	if ( tac != 3 ) {
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}
	if ( present ) {
	    if ( unlink( path ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 0;
	}
	if ( symlink( decode( targv[ 2 ] ), path ) != 0 ) {
	    perror( path );
	    return( 1 );
	}
	if ( !quiet ) printf( "%s: symbolic linked to %s",
	    displaypath, decode( targv[ 2 ] ));
	break;

    case 'p':
	if ( tac != 5 ) { 
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}
	mode = strtol( targv[ 2 ], (char **)NULL, 8 );
	uid = atoi( targv[ 3 ] );
	gid = atoi( targv[ 4 ] );
	if ( !present ) {
	    mode = mode | S_IFIFO;
	    if ( mknod( path, mode, NULL ) != 0 ){
		perror( path );
		return( 1 );
	    }
	    if ( lstat( path, st ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 1;
	    newfile = 1;
	}
	/* check mode */
	if ( !quiet ) {
	    if ( newfile ) {
		printf( "%s: created updating", displaypath );
	    } else {
		printf( "%s: updating", displaypath );
	    }
	}
	if( mode != st->st_mode ) {
	    if ( chmod( path, mode ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( !quiet ) printf( " mode" );
	}
	/* check uid & gid */
	if( uid != st->st_uid  || gid != st->st_gid ) {
	    if ( chown( path, uid, gid ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( uid != st->st_uid ) {
		if ( !quiet ) printf( " uid" );
	    }
	    if ( gid != st->st_gid ) {
		if ( !quiet ) printf( " gid" );
	    }
	}
	break;

    case 'b':
    case 'c':
	if ( tac != 7 ) {
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}

	if ( present && ( ( minor( st->st_rdev )
		!= atoi( targv[ 6 ] ) ) || ( major( st->st_rdev )
		!= atoi( targv[ 5 ] ) ) ) ) {
	    if ( unlink( path ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 0;
	}

	mode = strtol( targv[ 2 ], (char **)NULL, 8 );
	if( *targv[ 0 ] == 'b' ) {
	    mode = mode | S_IFBLK;
	} else {
	    mode = mode | S_IFCHR;
	}
	uid = atoi( targv[ 3 ] );
	gid = atoi( targv[ 4 ] );
	if ( !present ) {
#ifdef sun
	    if ( ( dev = makedev( (major_t)atoi( targv[ 5 ] ),
		   (minor_t)atoi( targv[ 6 ] ) ) ) == NODEV ) {
	       perror( path );
	       return( 1 );
	    }
#else !sun
	    dev = makedev( atoi( targv[ 5 ] ), atoi( targv[ 6 ] ));
#endif sun
	    if ( mknod( path, mode, dev ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( lstat( path, st ) !=  0 ) {
		perror( path );
		return( 1 );
	    }
	    present = 1;
	    newfile = 1;
	}
	/* check mode */
	if ( !quiet ) printf( "%s: updating", path );
	if( mode != st->st_mode ) {
	    if ( chmod( path, mode ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( !quiet ) printf( " mode" );
	}
	/* check uid & gid */
	if( uid != st->st_uid  || gid != st->st_gid ) {
	    if ( chown( path, uid, gid ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( uid != st->st_uid ) {
		if ( !quiet ) printf( " uid" );
	    }
	    if ( gid != st->st_gid ) {
		if ( !quiet ) printf( " gid" );
	    }
	}
	break;
    case 's':
    case 'D':
	if ( tac != 5 ) { 
	    fprintf( stderr,
		"%d: incorrect number of arguments\n", linenum );
	    return( 1 );
	}
	mode = strtol( targv[ 2 ], (char **)NULL, 8 );
	uid = atoi( targv[ 3 ] );
	gid = atoi( targv[ 4 ] );
	if ( !present ) {
	    fprintf( stderr, "%d: Warning: %c %s not created...continuing\n",
		    linenum, *targv[ 0 ], path );
	    break;
	}
	/* check mode */
	if ( !quiet ) {
	    if ( newfile ) {
		printf( "%s: created updating", displaypath );
	    } else {
		printf( "%s: updating", displaypath );
	    }
	}
	if( mode != st->st_mode ) {
	    if ( chmod( path, mode ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( !quiet ) printf( " mode" );
	}
	/* check uid & gid */
	if( uid != st->st_uid  || gid != st->st_gid ) {
	    if ( chown( path, uid, gid ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( uid != st->st_uid ) {
		if ( !quiet ) printf( " uid" );
	    }
	    if ( gid != st->st_gid ) {
		if ( !quiet ) printf( " gid" );
	    }
	}
	break;
    default :
	printf( "%d: Unkown type %s\n", linenum, targv[ 0 ] );
	return( 1 );
    }

    if ( !quiet ) printf( "\n" );

    return( 0 );
}
