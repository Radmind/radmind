#include <sys/stat.h>

#define T_EOF      0  

#define T_POSITIVE   1
#define T_NEGATIVE   -1
#define T_SPECIAL    0

#define T_MOVE_TRAN  1
#define T_MOVE_FS    -1
#define T_MOVE_BOTH  0

#define T_MODE	     0x0FFF

#define FS2TRAN	0
#define TRAN2FS 1

#define FLAG_SKIP    ( 1<<0 )
#define FLAG_INIT    ( 1<<1 )

int	edit_path;
FILE    *outtran;

struct info {
    char		    i_name[ MAXPATHLEN ];
    char		    i_link[ MAXPATHLEN ];
    struct stat		    i_stat;
    int			    i_chksum;
#ifdef notdef
    major_t		    i_maj;
    minor_t		    i_min;
#endif
    dev_t		    i_dev;
    char		    i_type;
};

struct transcript {
    struct info		    t_info;
    struct transcript       *t_next;
    int		    	    t_type;
    int			    t_form;
    char		    t_name[ MAXPATHLEN ];
    mode_t		    t_mode;
    int			    t_flag;
    FILE		    *t_in;
};
 
int transcript( struct info *, char *, FILE * );
void transcript_init( int );
void transcript_free( void );
