#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#include "list.h"

/* Just like strcmp(), but pays attention to the meaning of '/'.  */
    static int
pathcmp( const unsigned char *p1, const unsigned char *p2 )
{
    int		rc;

    do {
	if (( rc = ( *p1 - *p2 )) != 0 ) {
	    if (( *p2 != '\0' ) && ( *p1 == '/' )) {
		return( -1 );
	    } else if (( *p1 != '\0' ) && ( *p2 == '/' )) {
		return( 1 );
	    } else {
		return( rc );
	    }
	}
	p2++;
    } while ( *p1++ != '\0' );

    return( 0 );
}

    void 
print_list( struct node *node )
{
    while ( node != NULL ) {
	printf( "%s\n", node->path);
	node = node->next;
    }
}

   struct node *
create_node( char *path )
{
    struct node 	*new_node;

    new_node = (struct node *) malloc( sizeof( struct node ) );
    new_node->path = strdup( path ); 

    return( new_node );
}

   void 
insert_node( char *path, struct node **head )
{
    struct node		*new_node, **previous, *node;
    int 		cmpval;

    new_node = create_node( path );

    node = *head;
    previous = head;

    while ( node != NULL ) {

	cmpval = pathcmp( node->path, new_node->path );
	if ( cmpval > 0 ) {
	    break;
	}
	previous = &( node->next );
	node = node->next;
    }
    new_node->next = *previous;
    *previous= new_node;
}
