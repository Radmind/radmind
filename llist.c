/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "applefile.h"
#include "transcript.h"
#include "llist.h"

/* Allocate a new list node */
    struct llist *
ll_allocate( char *name ) 
{
    struct llist	*new;

    /* allocate space for next item in list */
    if (( new = (struct llist *)malloc( sizeof( struct llist ))) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    } 

    /* copy info into new item */
    strcpy( new->ll_name, name );
    new->ll_next = NULL;

    return new;
}

/* Free the whole list */
    void
ll_free( struct llist *head )
{
    struct llist	*next;
    
    for ( ; head != NULL; head = next ) {
	next = head->ll_next;
	free( head );
    }
}

    void 
ll_insert( struct llist **headp, struct llist *new ) 
{ 
    struct llist	**current;

    /* find where in the list to put the new entry */
    for ( current = headp; *current != NULL; current = &(*current)->ll_next) {
	if ( strcmp( new->ll_name, (*current)->ll_name ) <= 0 ) {
	    break;
	}
    }

    new->ll_next = *current;
    *current = new; 
    return; 
}

/* Insert a new node into the list */
    void 
ll_insert_case( struct llist **headp, struct llist *new ) 
{ 
    struct llist	**current;

    /* find where in the list to put the new entry */
    for ( current = headp; *current != NULL; current = &(*current)->ll_next) {
	if ( strcasecmp( new->ll_name, (*current)->ll_name ) <= 0 ) {
	    break;
	}
    }

    new->ll_next = *current;
    *current = new; 
    return; 
}
