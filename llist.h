#include <sys/stat.h>
#include <sys/param.h>

struct llist {
    struct info		ll_info;
    struct llist	*ll_next;
    int			ll_flag;
    char		type;
};

struct llist * ll_allocate( char * );
