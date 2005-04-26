/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

struct llist {
    char		ll_name[ MAXPATHLEN ];
    struct llist	*ll_next;
};

struct llist * ll_allocate( char * );
void ll_free( struct llist * );
void ll_insert( struct llist **, struct llist * );
void ll_insert_case( struct llist **, struct llist * );
