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

struct command {
    char	*c_name;
    int		(*c_func) ___P(( NET *, int, char *[] ));
};
