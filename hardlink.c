/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/param.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "applefile.h"
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
    int			i_flag;
};


static struct devlist	*dev_head = NULL;

static char		*i_insert( struct devlist *dev_head,
				struct pathinfo *pinfo );
static struct devlist	*d_insert( struct devlist **dev_head,
				struct pathinfo *pinfo );
void			hardlink_free( void );

    char *
hardlink( struct pathinfo *pinfo )
{
    struct devlist	*device;

    device = d_insert( &dev_head, pinfo );

    return( i_insert( device, pinfo ));
}

    static struct devlist * 
d_insert( struct devlist **dev_head, struct pathinfo *pinfo )
{
    struct devlist	*new, **cur;

    for ( cur = dev_head; *cur != NULL; cur = &(*cur)->d_next ) {
	if ( pinfo->pi_stat.st_dev <= (*cur)->d_dev ) {
	    break;
	}
    }

    if (( (*cur) != NULL ) && ( pinfo->pi_stat.st_dev == (*cur)->d_dev )) {
	return( *cur );
    }

    if (( new = ( struct devlist * ) malloc( sizeof( struct devlist ))) 
	    == NULL ) {
	perror( "d_insert malloc" );
	exit( 2 );
    }

    new->d_dev = pinfo->pi_stat.st_dev; 
    new->d_ilist = NULL;
    new->d_next = *cur;
    *cur = new;

    return( *cur );
}

    static char *
i_insert( struct devlist *dev_head, struct pathinfo *pinfo )
{
    struct inolist	*new, **cur;

    for ( cur = &dev_head->d_ilist; *cur != NULL; cur = &(*cur)->i_next ) {
	if ( pinfo->pi_stat.st_ino <= (*cur)->i_ino ) {
	    break;
	}
    }

    if (( (*cur) != NULL ) && ( pinfo->pi_stat.st_ino == (*cur)->i_ino )) {
	return( (*cur)->i_name );
    }

    if (( new = ( struct inolist * ) malloc( sizeof( struct inolist ))) 
	    == NULL ) {
	perror( "i_insert malloc" );
	exit( 2 );
    }

    if (( new->i_name = ( char * ) malloc( strlen( pinfo->pi_name ) + 1 ))
	    == NULL ) {
	perror( "i_insert malloc" );
	exit( 2 );
    }

    strcpy( new->i_name, pinfo->pi_name );
    new->i_ino = pinfo->pi_stat.st_ino;
    new->i_flag = 0;

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

    int
hardlink_changed( struct pathinfo *pinfo, int set )
{
    struct devlist	*dcur;
    struct inolist	*icur;

    for ( dcur = dev_head; dcur != NULL; dcur = dcur->d_next ) {
	if ( pinfo->pi_stat.st_dev <= dcur->d_dev ) {
	    break;
	}
    }
    
    if (( dcur == NULL ) || ( pinfo->pi_stat.st_dev != dcur->d_dev )) {
	fprintf( stderr, "hardlink_changed: %s: dev not found\n",
		pinfo->pi_name );
	exit( 2 );
    }

    for ( icur = dcur->d_ilist; icur != NULL; icur = icur->i_next ) {
	if ( pinfo->pi_stat.st_ino <= icur->i_ino ) {
	    break;
	}
    }

    if (( icur == NULL ) || ( pinfo->pi_stat.st_ino != icur->i_ino )) {
	fprintf( stderr, "hardlink_changed: %s: ino not found\n",
		pinfo->pi_name );
	exit( 2 );
    }

    if ( set ) {
	icur->i_flag = 1;
    }

    return( icur->i_flag );
}
