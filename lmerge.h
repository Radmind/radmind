struct tran 
{
    int			num;
    int			eof;
    int			tac;
    int			remove;
    char		*path;
    char		*name;
    char		*line;
    char		tline[ 2 * MAXPATHLEN ];
    char		**targv;
    FILE		*fs;
    ACAV		*acav;
    struct node 	*next;
};
int getnextline( struct tran *tran ); 
