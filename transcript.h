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

struct pathinfo {
    char		pi_type;
    char		pi_name[ MAXPATHLEN ];
    char		pi_link[ MAXPATHLEN ];
    struct stat		pi_stat;
    int			pi_chksum;
    dev_t		pi_dev;
};

struct transcript {
    struct transcript	*t_next;
    struct pathinfo	t_pinfo;
    int 		t_type;
    char		t_name[ MAXPATHLEN ];
    int			t_linenum;
    int			t_eof;
    FILE		*t_in;
};
 
int	transcript( struct pathinfo *, char * );
void	transcript_init( int, char * );
void	transcript_free( void );
char	*hardlink( struct pathinfo * );
void	hardlink_free( void );
