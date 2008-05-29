/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "pathcmp.h"

struct node *   _list_create_node( char *path );

   struct node *
_list_create_node( char *path )
{
    struct node 	*new_node;

    if ( strlen( path ) >= MAXPATHLEN ) {
	errno = ENAMETOOLONG;
	return( NULL );
    }

    if (( new_node = (struct node *) malloc( sizeof( struct node ))) == NULL ) {
	return( NULL );
    }
    memset( new_node, 0, sizeof( struct node ));
    sprintf( new_node->n_path, "%s", path );

    return( new_node );
}

    struct list *
list_new( void )
{
    struct list 	*list;

    if (( list = malloc( sizeof( struct list ))) == NULL ) {
	return( NULL );
    }
    
    memset( list, 0, sizeof( struct list ));
    return( list );
}

    void
list_clear( struct list *list )
{
    /* Remove items from tail of list */
    while ( list->l_tail != NULL ) {
	list_remove_tail( list );
    }
}

    void
list_free( struct list *list )
{
    list_clear( list );
    free( list );
}
	
    void 
list_print( struct list *list )
{
    struct node		*cur;
    u_int		i;

    printf( "count: %d\n", list->l_count );
    for ( cur = list->l_head, i = 1; cur != NULL; cur = cur->n_next, i++ ) {
	printf( "%d: %s ( prev %s next %s )\n", i, cur->n_path,
	    cur->n_prev ? cur->n_prev->n_path : "NULL",
	    cur->n_next ? cur->n_next->n_path : "NULL" );
    }
    printf( "\n" );
}

   int 
list_insert_case( struct list *list, char *path, int case_sensitive )
{
    struct node		*new_node, *cur;

    for ( cur = list->l_head; cur != NULL; cur = cur->n_next ) {
	if ( pathcasecmp( cur->n_path, path, case_sensitive ) > 0 ) {
	    break;
	}
    }

    /* Insert at tail or into empty list */
    if ( cur == NULL ) {
	return( list_insert_tail( list, path ));
    }

    /* Insert at head */
    if ( cur->n_prev == NULL ) {
	return( list_insert_head( list, path ));
    }

    /* Insert in middle */
    if (( new_node = _list_create_node( path )) == NULL ) {
	return( -1 );
    }
    new_node->n_next = cur;
    cur->n_prev->n_next = new_node;
    new_node->n_prev = cur->n_prev;
    cur->n_prev = new_node;

    list->l_count++;
    return( 0 );
}

    int
list_insert( struct list *list, char *path )
{
    return( list_insert_case( list, path, 1 ));
}

   int 
list_insert_head( struct list *list, char *path )
{
    struct node		*new_node;

    if (( new_node = _list_create_node( path )) == NULL ) {
	return( -1 );
    }

    if ( list->l_head == NULL ) {
	list->l_tail = new_node;
    } else {
	list->l_head->n_prev = new_node;
	new_node->n_next = list->l_head;
    }
    list->l_head = new_node;

    list->l_count++;
    return( 0 );
}

   int 
list_insert_tail( struct list *list, char *path )
{
    struct node		*new_node;

    if (( new_node = _list_create_node( path )) == NULL ) {
	return( -1 );
    }

    if ( list->l_tail == NULL ) {
	list->l_head = new_node;
    } else {
	list->l_tail->n_next = new_node;
	new_node->n_prev = list->l_tail;
    }
    list->l_tail = new_node;

    list->l_count++;
    return( 0 );
}

    int
list_remove( struct list *list, char *path )
{
    int			count = 0;
    struct node		*cur;

    for ( cur = list->l_head; cur != NULL; cur = cur->n_next ) {
	if ( pathcmp( cur->n_path, path ) == 0 ) {

	    if ( list->l_head == cur ) {
		list_remove_head( list );
		count++;
		
	    } else if ( list->l_tail == cur ) {
		list_remove_tail( list );
		count++;

	    } else {
		/* Remove item */
		cur->n_prev->n_next = cur->n_next;
		cur->n_next->n_prev = cur->n_prev;
		free( cur );
		list->l_count--;
		count++;
	    }
	}
    }

    return( count );
}

    void
list_remove_tail( struct list *list )
{
    struct node         *node;

    if (( node = list_pop_tail( list )) != NULL ) {
        free( node );
    }
}

    struct node *
list_pop_tail( struct list * list )
{
    struct node		*node;

    if ( list->l_tail == NULL ) {
	return( NULL );
    }
    node = list->l_tail;
    if ( list->l_count == 1 ) {
	list->l_tail = NULL;
	list->l_head = NULL;
    } else {
	list->l_tail = list->l_tail->n_prev;
	list->l_tail->n_next = NULL;
    }
    list->l_count--;
    return( node );
}

    void
list_remove_head( struct list *list )
{
    struct node		*node;

    if (( node = list_pop_head( list )) != NULL ) {
	free( node );
    }
}

    struct node *
list_pop_head( struct list *list )
{
    struct node		*node;

    if ( list->l_head == NULL ) {
	return( NULL );
    }
    node = list->l_head;
    if ( list->l_count == 1 ) {
	list->l_tail = NULL;
	list->l_head = NULL;
    }
    if ( list->l_head != NULL ) {
	list->l_head = list->l_head->n_next;
	list->l_head->n_prev = NULL;
    }
    list->l_count--;
    return( node );
}

    int
list_check( struct list *list, char *path )
{
    struct node		*cur;

    for ( cur = list->l_head; cur != NULL; cur = cur->n_next ) {
	if ( pathcmp( cur->n_path, path ) == 0 ) {
	    return( 1 );
	}
    }

    return( 0 );
}
