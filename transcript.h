/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/stat.h>

#define T_NULL		0
#define T_POSITIVE	1
#define T_NEGATIVE	2 
#define T_SPECIAL	3

#define T_MOVE_TRAN	1
#define T_MOVE_FS	2
#define T_MOVE_BOTH	3 

#define T_MODE		0x0FFF

#define APPLICABLE	0
#define CREATABLE	1

#define PR_TRAN_ONLY	1  
#define PR_FS_ONLY	2
#define PR_DOWNLOAD	3 
#define PR_STATUS	4 
#define PR_STATUS_NEG	5

int			edit_path;
int			skip;
int			cksum;
FILE			*outtran;

struct pathinfo {
    char			pi_type;
    char			pi_name[ MAXPATHLEN ];
    char			pi_link[ MAXPATHLEN ];
    struct stat			pi_stat;
    char			pi_cksum_b64[ MAXPATHLEN ];
    struct applefileinfo	afinfo;
};

struct transcript {
    struct transcript	*t_next;
    struct pathinfo	t_pinfo;
    int 		t_type;
    char		t_fullname[ MAXPATHLEN ];
    char		t_shortname[ MAXPATHLEN ];
    int			t_linenum;
    int			t_eof;
    FILE		*t_in;
};

int	transcript( struct pathinfo * );
void	transcript_init( char *, char * );
void	transcript_free( void );
char	*hardlink( struct pathinfo * );
int	hardlink_changed( struct pathinfo *, int );
void	hardlink_free( void );
