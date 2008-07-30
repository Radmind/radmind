/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/stat.h>

#define T_NULL		0
#define T_POSITIVE	1
#define T_NEGATIVE	2 
#define T_SPECIAL	3

#define T_RELATIVE	0
#define T_ABSOLUTE	1

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
#define PR_STATUS_MINUS	6

#define K_CLIENT	0
#define K_SERVER	1

extern int		edit_path;
extern int		skip;
extern int		cksum;
extern int		fs_minus;
extern FILE		*outtran;
extern char		*path_prefix;

struct pathinfo {
    char			pi_type;
    int				pi_minus;
    char			pi_name[ MAXPATHLEN ];
    char			pi_link[ MAXPATHLEN ];
    struct stat			pi_stat;
    char			pi_cksum_b64[ MAXPATHLEN ];
    struct applefileinfo	pi_afinfo;
};

struct transcript {
    struct transcript	*t_next;
    struct transcript	*t_prev;
    struct pathinfo	t_pinfo;
    int 		t_type;
    int			t_num;
    char		t_fullname[ MAXPATHLEN ];
    char		t_shortname[ MAXPATHLEN ];
    char		t_kfile[ MAXPATHLEN ];
    int			t_linenum;
    int			t_eof;
    FILE		*t_in;
};

int			transcript( char *, struct stat *, char *, struct applefileinfo *, int );
void			transcript_init( char *kfile, int location );
struct transcript	*transcript_select( void );
void			transcript_parse( struct transcript * );
void			transcript_free( void );
void			t_new( int, char *, char *, char * );
int			t_exclude( char *path );
void			t_print( struct pathinfo *, struct transcript *, int );
char			*hardlink( struct pathinfo * );
int			hardlink_changed( struct pathinfo *, int );
void			hardlink_free( void );
