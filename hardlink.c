#include <stdlib.h>
#include <stdio.h>

#ifdef SOLARIS
#include <sys/mkdev.h>
#endif

#include <sys/param.h>

#include "transcript.h"

struct devlist {
    struct devlist	*d_next;
    dev_t		d_dev;
    struct inolist	*d_ilist;
};

struct inolist {
    struct inolist	*i_next;
    ino_t		i_ino;
    char		*i_name;
};


static struct devlist	*dev_head = NULL;

static char		*i_insert( struct devlist *dev_head,
				struct info *info );
static struct devlist	*d_insert( struct devlist **dev_head,
				struct info *info );
void			hardlink_free( void );

    char *
hardlink( struct info *info )
{
    struct devlist	*device;

    device = d_insert( &dev_head, info );

    return( i_insert( device, info ));
}

    static struct devlist * 
d_insert( struct devlist **dev_head, struct info *info )
{
    struct devlist	*new, **cur;

    for ( cur = dev_head; *cur != NULL; cur = &(*cur)->d_next ) {
	if ( info->i_stat.st_dev <= (*cur)->d_dev ) {
	    break;
	}
    }
    
    if (( (*cur) != NULL ) && ( info->i_stat.st_dev == (*cur)->d_dev )) {
	return( *cur );
    }

    if (( new = ( struct devlist * ) malloc( sizeof( struct devlist ))) 
	    == NULL ) {
	perror( "d_insert malloc" );
	exit( 1 );
    }

    new->d_dev = info->i_stat.st_dev; 
    new->d_ilist = NULL;
    new->d_next = *cur;
    *cur = new;

    return( *cur );
}

    static char *
i_insert( struct devlist *dev_head, struct info *info )
{
    struct inolist	*new, **cur;

    for ( cur = &dev_head->d_ilist; *cur != NULL; cur = &(*cur)->i_next ) {
	if ( info->i_stat.st_ino <= (*cur)->i_ino ) {
	    break;
	}
    }

    if (( (*cur) != NULL ) && ( info->i_stat.st_ino == (*cur)->i_ino )) {
	return( (*cur)->i_name );
    }
    
    if (( new = ( struct inolist * ) malloc( sizeof( struct inolist ))) 
	    == NULL ) {
	perror( "i_insert malloc" );
	exit( 1 );
    }

    if (( new->i_name = ( char * ) malloc( strlen( info->i_name ) + 1 ))
	    == NULL ) {
	perror( "i_insert malloc" );
	exit( 1 );
    }

    strcpy( new->i_name, info->i_name );
    new->i_ino = info->i_stat.st_ino;

    new->i_next = *cur;
    *cur = new;

    return( NULL );

}

    void
hardlink_free( )
{
    struct devlist	*dev_next;
    struct inolist	*ino_head, *ino_next;

    while ( dev_head != NULL ) {
	dev_next = dev_head->d_next;
	ino_head = dev_head->d_ilist;
	while ( ino_head != NULL ) {
	    ino_next = ino_head->i_next;
	    free( ino_head->i_name);
	    free( ino_head );
	    ino_head = ino_next;
	}
	free( dev_head );
	dev_head = dev_next;
    }
}
