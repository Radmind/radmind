/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef __STDC__
#define ___P(x)		x
#else __STDC__
#define ___P(x)		()
#endif __STDC__

int		cmdloop ___P(( int ));
int		command_k ___P(( char * ));
char		**special_t ___P(( char *, char * ));
void		do_chksum ___P(( char *, char * ));
int		keyword ___P(( int, char*[] ));
int		create_directories ___P (( char * ));
extern char	*path_radmind;
extern char	*remote_host;

struct command {
    char	*c_name;
    int		(*c_func) ___P(( SNET *, int, char *[] ));
};
