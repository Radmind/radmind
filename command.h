/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef __STDC__
#define ___P(x)		x
#else __STDC__
#define ___P(x)		()
#endif __STDC__

int		cmdloop ___P(( int, struct sockaddr_in * ));
int		command_k ___P(( char * ));
char		**special_t ___P(( char *, char * ));
int		keyword ___P(( int, char*[] ));
extern char	*path_radmind;

struct command {
    char	*c_name;
    int		(*c_func) ___P(( SNET *, int, char *[] ));
};

int wildcard( char *key, char *string );
