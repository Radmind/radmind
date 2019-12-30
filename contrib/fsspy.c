/*
 * Copyright 2008 The Regents of The University of Michigan
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * Written by Andrew Mortensen, Dec. 2008.
 */

/*
 * To compile:
  	gcc -g -framework CoreFoundation -framework CoreServices \
		-o fsspy fsspy.c
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/times.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int		errno;

struct event {
    char		*e_path;
    unsigned long long	e_id;
    unsigned long	e_flags;
    struct event	*e_next;
};

struct event		*event_head = NULL;

int			debug = 0;
int			case_sensitive = 1;
int			delayed_print = 0;
int			all_paths = 0;
char			*prefix = NULL;

    int
ischild( char *child, char *parent )
{
    int			rc, len;

    if ( parent == NULL ) {
	return( 1 );
    }

    if (( len = strlen( parent )) > strlen( child )) {
	return( 0 );
    }
    if ( len == 1 && *parent == '/' ) {
	return(( *child == '/' ));
    }

    if ( case_sensitive ) {
	rc = strncmp( parent, child, len );
    } else {
	rc = strncasecmp( parent, child, len );
    }
    if ( rc == 0 && ( child[ len ] == '/' || child[ len ] == '\0' )) {
	return( 1 );
    }

    return( 0 );
}

    struct event *
event_new( char *path, unsigned long long id, unsigned long flags )
{
    struct event	*new;
    int			len;

    if (( new = (struct event *)malloc( sizeof( struct event ))) == NULL ) {
	return( NULL );
    }
    
    /* user may have specified a prefix for the path. */
    len = strlen( path ) + 1;
    if ( prefix != NULL ) {
	len += strlen( prefix );
    }
    if (( new->e_path = (char *)malloc( len )) == NULL ) {
	free( new );
	return( NULL );
    }
    if ( prefix != NULL ) {
	strcpy( new->e_path, prefix );
	strcat( new->e_path, path );
    } else {
	strcpy( new->e_path, path );
    }

    /* the fsevents API returns paths with a trailing slash. chop it. */
    len = strlen( new->e_path );
    if ( new->e_path[ len - 1 ] == '/' ) {
	new->e_path[ len - 1 ] = '\0';
    }

    new->e_id = id;
    new->e_flags = flags;
    new->e_next = NULL;

    return( new );
}

    void
event_insert( struct event **head, struct event *new )
{
    struct event	**cur;
    struct event	*tmp;
    int			rc;

    /* must use while loop here since we're traversing and (maybe) deleting. */
    cur = head;

    while ( *cur != NULL ) {
	if ( !all_paths && ischild( new->e_path, (*cur)->e_path )) {
	    /* drop new node. we've already got the parent in the list. */
	    free( new->e_path );
	    free( new );
	    return;
	} else if ( !all_paths && ischild((*cur)->e_path, new->e_path )) {
	    /* the new node is the cur parent. drop cur. */
	    tmp = *cur;
	    *cur = (*cur)->e_next;
	    free( tmp->e_path );
	    free( tmp );
	    /* the deletion takes care of the increment. */
	} else {
	    if ( case_sensitive ) {
		rc = strcmp( new->e_path, (*cur)->e_path );
	    } else  {
		rc = strcasecmp( new->e_path, (*cur)->e_path );
	    }

	    if ( rc == 0 ) {
		/* new is already in the list. drop it. */
		free( new->e_path );
		free( new );
		return;
	    } else if ( rc < 0 ) {
		break;
	    } else {
		cur = &(*cur)->e_next;
	    }
	}
    }

    new->e_next = *cur;
    *cur = new;
}

    void
event_print( struct event **head )
{
    struct event	*cur, *tmp;

    for ( cur = *head; cur != NULL; cur = tmp ) {
	printf( "%s\n", cur->e_path );

	tmp = cur->e_next;

	free( cur->e_path );
	free( cur );
    }

    *head = NULL;
}

    void
fsevent_callback( ConstFSEventStreamRef ref, void *info,
			size_t nevents, void *eventpaths,
			const FSEventStreamEventFlags eflags[],
			const FSEventStreamEventId eids[] )
{
    struct event	*new;
    char		**paths = eventpaths;
    int			i;
    time_t		now;

#define FSEVENTID	"ID"
#define FSEVENTFLAGS	"Flags"
#define FSEVENTPATH	"Path"

    if ( debug && nevents > 0 ) {
	printf( "# %-8s %-10s %s\n", FSEVENTID, FSEVENTFLAGS, FSEVENTPATH );
    }

    for ( i = 0; i < nevents; i++ ) {
	if (( new = event_new( paths[ i ], eids[ i ],
			    eflags[ i ] )) == NULL ) {
	    perror( "event_new: malloc" );
	    exit( 2 );
	}
	event_insert( &event_head, new );

	if ( debug ) {
	  printf( "%-10llu %-10lu %s\n", eids[ i ], ( unsigned long )eflags[ i ], paths[ i ] );
	}
    }

    if ( nevents > 0 && !delayed_print ) {
	event_print( &event_head );
    }
}

/* called when the user-specified run time ends */
    void
timer_callback( CFRunLoopRef runloop, void *info )
{
    FSEventStreamRef	fsstream = (FSEventStreamRef)info;

    /* block until all events are cleared. */
    FSEventStreamFlushSync( fsstream );

    CFRunLoopStop( CFRunLoopGetCurrent());
}

    int
main( int ac, char *av[] )
{
    CFStringRef		*paths = NULL;
    CFArrayRef		watchpaths = NULL;
    FSEventStreamRef	fsstream = NULL;
    CFAbsoluteTime	latency = 2.0;
    CFRunLoopTimerRef	timer;
    SInt32		runloop_rc;
    int			watchtime = 0;
    int			c;
    int			i;
    int			err = 0;

    extern int		optind;
    extern char		*optarg;

    while (( c = getopt( ac, av, "adIl:p:t:w" )) != -1 ) {
	switch ( c ) {
	case 'a':		/* print all paths regardless of overlap. */
	    all_paths = 1;
	    break;

	case 'd':		/* debug */
	    debug = 1;
	    break;

	case 'I':		/* case-insensitive path comparisons */
	    case_sensitive = 0;
	    break;

	case 'l':		/* latency */
	    errno = 0;
	    latency = strtod( optarg, NULL );
	    if ( errno ) {
		fprintf( stderr, "strtod %s: %s\n", optarg, strerror( errno ));
		exit( 1 );
	    }
	    if ( latency < 0.0 ) {
		fprintf( stderr, "latency cannot be negative\n" );
		exit( 1 );
	    }
	    break;

	case 'p':		/* prefix for path output. */
	    if (( prefix = strdup( optarg )) == NULL ) {
		perror( "strdup" );
		exit( 2 );
	    }
	    break;

	case 't':		/* time in seconds to watch given paths */
	    errno = 0;
	    watchtime = strtoul( optarg, NULL, 10 );
	    if ( errno ) {
		fprintf( stderr, "strtoul %s: %s\n", optarg, strerror( errno ));
		exit( 1 );
	    }
	    break;

	case 'w':		/* print paths after watchtime elapses. */
	    delayed_print = 1;
	    break;

	default:
	    err++;
	    break;
	}
    }	

    if ( delayed_print && !watchtime ) {
	err++;
    }

    if ( err || ( ac - optind ) < 1 ) {
	fprintf( stderr, "usage: %s [ -adI ] [ -l delay_in_seconds ] "
			 "[ -p path_prefix ] [ -t run_time_in_seconds [ -w ]] "
			 "path1 [path2 path3 ... pathN]\n", av[ 0 ] );
	exit( 1 );
    }

    if (( paths = ( CFStringRef * )malloc(( ac - 1 ) * sizeof( CFStringRef )))
		== NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    for ( i = 1; i < ac; i++ ) {
	paths[ i - 1 ] = CFStringCreateWithCString( kCFAllocatorDefault,
					av[ i ], kCFStringEncodingUTF8 );
	if ( paths[ i - 1 ] == NULL ) {
	    fprintf( stderr, "%s: failed to create CFStringRef\n", av[ i ] );
	    exit( 2 );
	}
    }
    watchpaths = CFArrayCreate( kCFAllocatorDefault,
				( const void ** )paths,
				ac - 1,
				&kCFTypeArrayCallBacks );
    if ( watchpaths == NULL ) {
	fprintf( stderr, "CFArrayCreate failed\n" );
	exit( 2 );
    }

    for ( i = 0; i < ( ac - 1 ); i++ ) {
	CFRelease( paths[ i ] );
    }
    free( paths );

    fsstream = FSEventStreamCreate( kCFAllocatorDefault, &fsevent_callback,
			NULL, watchpaths,
			kFSEventStreamEventIdSinceNow,
			latency, kFSEventStreamCreateFlagNone );
    if ( fsstream == NULL ) {
	fprintf( stderr, "FSEventStreamCreate failed\n" );
	exit( 2 );
    }

    /* line buffering to make sure we get the output */
    setlinebuf( stdout );

    FSEventStreamScheduleWithRunLoop( fsstream,
		CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );
    if ( !FSEventStreamStart( fsstream )) {
	fprintf( stderr, "FSEventStreamStart failed\n" );
	exit( 2 );
    }

    if ( watchtime ) {
	CFRunLoopTimerContext context = { 0, fsstream, NULL, NULL, NULL };
	timer = CFRunLoopTimerCreate( kCFAllocatorDefault,
					watchtime + CFAbsoluteTimeGetCurrent(),
					0, 0, 0,
					(CFRunLoopTimerCallBack)timer_callback,
					&context );
	if ( timer == NULL ) {
	    fprintf( stderr, "CFRunLoopTimerCreate failed\n" );
	    exit( 2 );
	}
	CFRunLoopAddTimer( CFRunLoopGetCurrent(),
				timer, kCFRunLoopDefaultMode );
    }

    CFRunLoopRun();

    if ( delayed_print ) {
	event_print( &event_head );
    }

    FSEventStreamStop( fsstream );
    FSEventStreamInvalidate( fsstream );
    FSEventStreamRelease( fsstream );

    if ( prefix != NULL ) {
	free( prefix );
    }

    return( 0 );
}
