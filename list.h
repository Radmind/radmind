/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

struct list
{
    int			l_count;
    struct node		*l_head;	
    struct node		*l_tail;	
};

struct node
{
    char 		n_path[ MAXPATHLEN ];
    struct node 	*n_next;
    struct node 	*n_prev;
};

struct list *	list_new( void );
void		list_clear( struct list *list );
void		list_free( struct list *list );
void 		list_print( struct list *list );
int 		list_insert( struct list *list, char *path );
int 		list_insert_head( struct list *list, char *path );
int 		list_insert_tail( struct list *list, char *path );
void 		list_remove_head( struct list *list );
void 		list_remove_tail( struct list *list );
struct node *	list_pop_head( struct list *list );
struct node *	list_pop_tail( struct list *list );
int		list_check( struct list *list, char *path );
