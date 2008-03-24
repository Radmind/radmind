/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

int		cmdloop( int, struct sockaddr_in * );
int		command_k( char *, int );
char		**special_t( char *, char * );
int		keyword( int, char*[] );
extern char	*path_radmind;

struct command {
    char	*c_name;
    int		(*c_func)( SNET *, int, char *[] );
};
