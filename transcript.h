#include <sys/stat.h>
#include <sys/param.h>

#define T_EOF      0  
#define NOPRINT    0
#define PRINT	   1 
#define POS	   1
#define NEG	   0
#define SPECIAL    2
#define NOMOVE	   0
#define MOVE	   1

struct info {
    char		    name[ MAXPATHLEN ];
    struct stat		    stat;
    char		    link[ MAXPATHLEN ];
    int			    chksum;
    major_t		    maj;
    minor_t		    min;
};

struct transcript {
    struct info		    t_info;
    struct transcript       *t_next;
    int		    	    t_type;
    char		    file_name[ MAXPATHLEN ];
    mode_t		    type;
    int			    t_flag;
    FILE		    *t_in;
};
