#include <sys/stat.h>
#include <sys/param.h>

struct llist {
    struct pathinfo	ll_pinfo;
    struct llist	*ll_next;
    int			ll_flag;
    char		type;
};

struct llist * ll_allocate( char * );
void ll_free( struct llist * );
void ll_insert( struct llist **, struct llist * );
