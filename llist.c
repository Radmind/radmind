#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include "transcript.h"
#include "llist.h"

/* Allocate a new list node */
    struct llist *
ll_allocate( char *name ) 
{
    struct llist 	*new;

    /* allocate space for next item in list */
    if (( new = (struct llist *)malloc( sizeof( struct llist ))) == NULL ) {
	perror( "malloc" );
	exit( 1 );
    } 

    /* copy info into new item */
    strcpy( new->ll_info.i_name, name );
    new->ll_next = NULL;
    new->ll_info.i_chksum = 0;
    new->ll_flag = 0;

    return new;
}

/* Free the whole list */
    void
ll_free( struct llist *head )
{
    struct llist	*next;
    
    for ( ; head != NULL; head = next ) {
        next = head->ll_next;
	free( head->ll_info.i_name );
        free( head );
    }
}

/* Insert a new node into the list */
    void 
ll_insert( struct llist **headp, struct llist *new ) 
{ 
    struct llist	**current;
    int			ret = 0; 

    /* find where in the list to put the new entry */
    for ( current = headp; *current != NULL; current = &(*current)->ll_next) {
	ret = strcmp( new->ll_info.i_name, (*current)->ll_info.i_name );
	if ( ret <= 0 ) {
	    break;
	}
    }

    new->ll_next = *current;
    *current = new; 
    return; 
} 
