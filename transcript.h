#include <sys/stat.h>

#define T_NULL		0
#define T_POSITIVE	1
#define T_NEGATIVE	2 
#define T_SPECIAL	3

#define T_MOVE_TRAN	1
#define T_MOVE_FS	2
#define T_MOVE_BOTH	3 

#define T_MODE		0x0FFF

#define FS2TRAN		0
#define TRAN2FS		1

#define FLAG_SKIP	( 1 << 0 )
#define FLAG_INIT	( 1 << 1 )

int			edit_path;
FILE			*outtran;

struct info {
    char		i_type;
    char		i_name[ MAXPATHLEN ];
    char		i_link[ MAXPATHLEN ];
    struct stat		i_stat;
    int			i_chksum;
    dev_t		i_dev;
};

struct transcript {
    struct transcript	*t_next;
    struct info		t_info;
    int 		t_type;
    char		t_name[ MAXPATHLEN ];
    int			t_linenum;
    int			t_eof;
    FILE		*t_in;
};
 
int	transcript( struct info *, char * );
void	transcript_init( int );
void	transcript_free( void );
char	*hardlink( struct info *info );
void	hardlink_free( void );
