struct node
{
    char 		*path;
    struct node 	*next;
};
void print_list( struct node *node );
int compare_name( struct node *a, struct node *b );
struct node * create_node( char *path );
void insert_node( char *path, struct node **head );
