struct node
{
    char 		*path;
    struct node 	*next;
};
void print_list( struct node *node );
void insert_tail( char *path, struct node **head );
void insert_head( char *path, struct node **head );
void remove_head( struct node **head, char *path );
void free_list( struct node **head );
