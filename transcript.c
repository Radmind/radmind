#include <stdio.h>
#include <sys/mkdev.h>
#include <sys/stat.h>

#include "transcript.h"
#include "llist.h"
#include "code.h"

static struct transcript *tran_head;
static FILE 		 *com;

    static void 
t_parse( struct transcript *tran ) 
{
    char	        	line[ MAXPATHLEN ];
    int				length;
    int				llength;
    char			temp[ MAXPATHLEN ]; 
    char			*buf;
    int				i = 0, b = 0;
    char			**argv;
    char			c;
    int				ac;

    if ( tran == NULL ) {
	return;
    }

    if (( fgets( line, MAXPATHLEN, tran->t_in )) == NULL ) {
	tran->t_flag = T_EOF;
	return;
    }

    /* check to see if line contains the whole line */
    length = strlen( line );
    if ( line[ length - 1 ] != '\n' ) {
	fprintf( stderr, "ERROR: didn't get the whole line\n" );
	exit( 1 );
    } 
    line[ length - 1 ] = '\0';

    ac = argcargv( line, &argv );

    if ( strlen( argv[ 0 ] ) != 1 ) {
	fprintf( stderr, "ERROR: Incorrect form of transcript\n" );
	exit( 1 );
    }

    /* reading */
    switch( *argv[ 0 ] ) {
    case 'd':				    /* dir */
	if ( ac != 5 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	tran->t_info.stat.st_mode = strtol( argv[ 1 ], NULL, 8 );
	tran->t_info.stat.st_uid = atoi( argv[ 2 ] );
	tran->t_info.stat.st_gid = atoi( argv[ 3 ] );
	buf = decode( argv[ 4 ] );
	strcpy( tran->t_info.name, buf );
	break;

    case 'b':				    /* block or char */
    case 'c':
    case 'p':
    case 'D':
    case 's':
	if ( ac != 7 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	tran->t_info.stat.st_mode = strtol( argv[ 1 ], NULL, 8 );
	tran->t_info.stat.st_uid = atoi( argv[ 2 ] );
	tran->t_info.stat.st_gid = atoi( argv[ 3 ] );
	tran->t_info.maj = atoi( argv[ 4 ] );
	tran->t_info.min = atoi( argv[ 5 ] );
	buf = decode( argv[ 6 ] );
	strcpy( tran->t_info.name, buf );
	break;

    case 'l':				    /* link */
	if ( ac != 4 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	tran->t_info.stat.st_mode = strtol( argv[ 1 ], NULL, 8 );
	buf = decode( argv[ 2 ] );
	strcpy( tran->t_info.link, buf );
	buf = decode( argv[ 3 ] );
	strcpy( tran->t_info.name, buf );
	break;

    case 'f':				    /* file */
	if ( ac != 8 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	tran->t_info.stat.st_mode = strtol( argv[ 1 ], NULL, 8 );
	tran->t_info.stat.st_uid = atoi( argv[ 2 ] );
	tran->t_info.stat.st_gid = atoi( argv[ 3 ] );
	tran->t_info.stat.st_ctime = atoi( argv[ 4 ] );
	tran->t_info.stat.st_size = atoi( argv[ 5 ] );
	tran->t_info.chksum = atoi( argv[ 6 ] );
	buf = decode( argv[ 7 ] );
	strcpy( tran->t_info.name, buf );
	break;

    default:
	fprintf( stderr, "ERROR: Incorrect file type\n" );
	break;
    }

    tran->t_info.chksum = 0;
    return;
}

    static void
t_printfs( struct info *cur, FILE *out, char *rpath )
{
    char 	*buf;
    char	*link;
    int		type;
    major_t	maj;
    minor_t	min;

    /* encode the name */
    buf = encode( cur->name );
    type = S_IFMT & cur->stat.st_mode;

    /* print out info to file based on type */
    switch( type ) {
    case S_IFDIR:
	fprintf( out, "d %6o %5d %5d", cur->stat.st_mode, 
		cur->stat.st_uid, cur->stat.st_gid );
	if ( strcmp( rpath, cur->name ) != 0 ) {
		fprintf( out, " /%s\n", buf );
	} else {
		fprintf( out, " %s\n", buf );
	}
	break;
    case S_IFLNK:
	link = encode( cur->link );
	fprintf( out, "l %6o %s", cur->stat.st_mode, link );
	buf = encode( cur->name );
	fprintf( out, " /%s\n", buf );
	break;
    case S_IFREG:
	fprintf( out, "f %6o %5d %5d %9d %7d %3d /%s\n", 
		cur->stat.st_mode, cur->stat.st_uid,
		cur->stat.st_gid, cur->stat.st_ctime,
		cur->stat.st_size, cur->chksum,
		buf );
	break;
    default:
	maj = major( cur->stat.st_rdev );
	min = minor( cur->stat.st_rdev );
	if ( S_ISBLK( cur->stat.st_mode )) {
		fprintf( out, "b " );
	} else if ( S_ISCHR( cur->stat.st_mode )) {
		fprintf( out, "c " );
	} else if ( S_ISDOOR( cur->stat.st_mode )) {
		fprintf( out, "D " );
	} else if ( S_ISFIFO( cur->stat.st_mode )) {
		fprintf( out, "p " );
	} else if ( S_ISSOCK( cur->stat.st_mode )) {
		fprintf( out, "s " );
	} else { 
		fprintf( stderr, "ERROR: Incorrect file type\n" );
		break;
	}
	fprintf( out, "%o %4d %5d %5d %5d /%s\n", 
		cur->stat.st_mode, cur->stat.st_uid,
		cur->stat.st_gid, maj, min, buf );
	break;
    } 
}

   static int 
t_compare( struct llist *cur, struct transcript *tran, char *rpath,
		FILE *out ) 
{
    int		    		ret = -1;
    char	    		name[ MAXPATHLEN ];
    mode_t    	    		mode;
    mode_t	    		type;
    mode_t	    		tran_mode;
    mode_t	    		tran_type;
    major_t         		maj;
    minor_t         		min;

    if ( strcmp( cur->ll_info.name, rpath ) == 0 ) {
	strcpy( name, cur->ll_info.name );
    } else {
        sprintf( name, "/%s", cur->ll_info.name );
    }

    /* writing */
    if ( tran == NULL ) {
	ret = -1;
    } else if ( tran->t_flag != T_EOF ) {
	ret = strcmp( name, tran->t_info.name );
    }

    if ( ret > 0 ) {
	/* name is in the tran, but not the fs */
	t_printfs( &cur->ll_info, out, rpath );
	return 1;
    } 
    if ( ret < 0 ) {
	/* name is in the fs, but not in the tran */
	t_printfs( &cur->ll_info, out, rpath );
	return -1;
    } 

    /* the names match so check types */
    mode = ( S_IAMB & cur->ll_info.stat.st_mode );
    type = ( S_IFMT & cur->ll_info.stat.st_mode );
    tran_mode = ( S_IAMB & tran->t_info.stat.st_mode );
    tran_type = ( S_IFMT & tran->t_info.stat.st_mode );
    if ( tran_type != type ) {
        fprintf( stderr, "ERROR: Incorrect file type!\n" );
        return -1;
    }

    switch( tran_type ) {
    case S_IFREG:			    /* file */
        if (( cur->ll_info.stat.st_uid != tran->t_info.stat.st_uid ) || 
    		( cur->ll_info.stat.st_gid != tran->t_info.stat.st_gid ) ||
    		( cur->ll_info.stat.st_ctime != tran->t_info.stat.st_ctime ) ||
    		( cur->ll_info.stat.st_size != tran->t_info.stat.st_size ) || 
    		( cur->ll_info.chksum != tran->t_info.chksum ) ||
		( mode != tran_mode )) {
			t_printfs( &cur->ll_info, out, rpath );
    	}
	break;

    case S_IFDIR:				/* dir */
        if (( cur->ll_info.stat.st_uid != tran->t_info.stat.st_uid ) ||
	    	( cur->ll_info.stat.st_gid != tran->t_info.stat.st_gid ) ||
		( mode != tran_mode )) {
			t_printfs( &cur->ll_info, out, rpath );
        }
        break;

    case S_IFLNK:			    /* link */
        if ((( strcmp( cur->ll_info.link, tran->t_info.link )) != 0 )  ||
		( mode != tran_mode )) {
			t_printfs( &cur->ll_info, out, rpath );
	} 
	break;
    case S_IFCHR:
    case S_IFDOOR:
    case S_IFIFO:
    case S_IFSOCK:
	maj = major( cur->ll_info.stat.st_rdev );
	min = minor( cur->ll_info.stat.st_rdev );
	if (( cur->ll_info.stat.st_uid != tran->t_info.stat.st_uid ) ||
	    	( cur->ll_info.stat.st_gid != tran->t_info.stat.st_gid ) || 
	    	( maj != tran->t_info.maj ) || 
	    	( min != tran->t_info.min ) ||
		( mode != tran_mode )) {
			t_printfs( &cur->ll_info, out, rpath );
	}	
	break;
    default:
	fprintf( stderr, "ERROR: Incorrect file type\n" );
	break;
    }

    return 0;
}

    int
transcript( struct llist *new, char *name, char *rpath, FILE *out )
{

    int			move = TRUE;
    int			pos = FALSE;
    int 		count;
    char		buf[ MAXPATHLEN ];
    char		temp[ MAXPATHLEN ];
    int			ret;
    int			comp;
    struct transcript	*t_cur = NULL;
    struct transcript	*begin_tran = NULL;

printf( "in tran\n" );

    if ( lstat( name, &new->ll_info.stat ) != 0 ) {
	perror( name );
	exit( 1 );
    }

    /* check to see if a link, then read it in */
    if ( S_ISLNK( new->ll_info.stat.st_mode )) {
	count = readlink( new->ll_info.name, buf, MAXPATHLEN );
	buf[ count ] = '\0';
	strcpy( new->ll_info.link, buf );
    }

    if ( S_ISDIR( new->ll_info.stat.st_mode )) {
	move = MOVE;
    } else { 
	move = NOMOVE;
    }

    for ( ; ; ) {
    	/* find the correct transcript to start with */
    	if ( tran_head != NULL ) {
		for ( begin_tran = tran_head, t_cur = tran_head->t_next;
				t_cur != NULL; t_cur = t_cur->t_next ) {
			/* compare heads of trans */
			if ( t_cur->t_flag != T_EOF ) {
				comp = strcmp( begin_tran->t_info.name, 
					t_cur->t_info.name );
				if (( comp > 0 ) || ( comp < 0 )) {
					begin_tran = t_cur;
printf( "begin tran is now: %s\n", begin_tran->t_info.name );
					break;
   				}
			}
		}
     	}

     	/* move ahead other transcripts that match */
	if ( begin_tran != NULL ) {
     	   for( t_cur = begin_tran->t_next; t_cur != NULL; 
			t_cur = t_cur->t_next ) {
		if ( strcmp( begin_tran->t_info.name, t_cur->t_info.name ) 
					== 0 ) {
			t_parse( t_cur );
		}
     	   }
	}

	ret = t_compare( new, begin_tran, rpath, out );
	if (( ret != -1 ) && ( begin_tran != NULL )) {
		t_parse( begin_tran );
		if (( begin_tran->t_type == NEG ) && ( ret == 0 )) {
			pos = FALSE;
        	}
		break;
	} else if (( ret == -1 )) {
		break;
	}			

     }

    if (( move == TRUE ) && ( pos == TRUE )) {
	return POS;
    } else {
printf( "returning neg for: %s\n", new->ll_info.name );
	return NEG;
    }
}

    static struct transcript *
t_new( char *name, struct transcript *head ) 
{
    struct transcript    *new;

    if (( new = (struct transcript *)malloc( sizeof( struct transcript )))
	    == NULL ) {
	perror( "malloc" );
	exit( 1 );
    }

    strcpy( new->t_info.name, name );
    new->t_next = head;

    return( new );
}

    void
transcript_init( )
{
    char	**av;
    char	line[ MAXPATHLEN ];

    tran_head = NULL;

    if (( com = fopen( "command", "r" )) != NULL ) {
	while ( fgets( line, sizeof( line ), com ) != NULL ) {
printf( "in while\n" );
		argcargv( line, &av );
		tran_head = t_new( av[ 1 ], tran_head );
		switch( *av[0] ) {
		case 'p':
			tran_head->t_type = POS;
			break;
		case 'n':
			tran_head->t_type = NEG;
			break;
		case 's':
			tran_head->t_type = SPECIAL;
			continue;
		default:
			fprintf( stderr, "Invalid type of transcript\n" );
			exit( 1 );
		}

		/* open transcript and parse the first line */
		if (( tran_head->t_in = fopen( av[ 1 ], "r" )) == NULL ) {
			perror( av[ 1 ] );
			exit( 1 );
    		}
		strcpy( tran_head->file_name, av[ 1 ] );
		tran_head->t_flag = MOVE;
		/*parse first line of this transcript */
		t_parse( tran_head );
	}
     }

     return;
}


    void
transcript_free( )
{
    struct transcript    *next;
    FILE		 *com;

    if ( tran_head == NULL ) { 
	return;
    }

    for ( ; tran_head != NULL; tran_head = next ) {
	next = tran_head->t_next;
	fclose( tran_head->t_in );
	free( tran_head );
    }
    free( com );
}
