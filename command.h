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
int		cmd_lookup ___P(( char * ));
char		**find_file ___P(( char *, char * ));
void		do_chksum ___P(( char *, char * ));
int		create_directories ___P (( char * ));
extern char	*path_radmind;
extern char	*c_hostname;

struct command {
    char	*c_name;
    int		(*c_func) ___P(( SNET *, int, char *[] ));
};
