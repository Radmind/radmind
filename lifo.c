#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "lifo.h"

struct node * create_node( char *path );
void free_node( struct node *node );

    void 
print_list( struct node *head)
{
    struct node	*node;

    node = head;
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
free_node( struct node *node )
{
    free( node->path );
    free( node );
}

    void
free_list( struct node **head )
{
    struct node	*node;

    while ( *head != NULL ) {
	node = *head;
	*head = (*head)->next;
	free_node( node );
    }
}

   void 
insert_tail( char *path, struct node **head )
{
    struct node		*new_node, *node;

    new_node = create_node( path );
    new_node->next = NULL;
    if ( *head  == NULL ) {
	*head = new_node;
    } else {
	node = *head;
	while( node->next != NULL ) {
	    node = node->next;
	}
	node->next = new_node;
    }
    return;
}

    void
insert_head( char *path, struct node ** head )
{
    struct node		*new_node;

    new_node = create_node( path );
    new_node->next = *head;
    *head = new_node;
}

    void
remove_head( struct node **head, char *path )
{
    struct node		*node;

    node = *head;
    *head = node->next;
    strncpy( path, node->path, MAXPATHLEN );
    free_node( node );
}
	
