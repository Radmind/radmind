/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

SNET * connectsn( char *host, unsigned short port );
int closesn( SNET *sn );
char **get_capabilities( SNET * );
#ifdef HAVE_ZLIB
int negotiate_compression( SNET *, char ** );
int print_stats( SNET * );
extern int zlib_level;
#endif /* HAVE_ZLIB */

int retr( SNET *sn, char *pathdesc, char *path, char *temppath,
    mode_t tempmode, off_t transize, char *trancksum );
int retr_applefile( SNET *sn, char *pathdesc, char *path, char *temppath,
    mode_t tempmode, off_t transize, char *trancksum );

int n_stor_file( SNET *sn, char *pathdesc, char *path );
int stor_file( SNET *sn, char *pathdesc, char *path, off_t transize,
    char *trancksum );
int n_stor_applefile( SNET *sn, char *pathdesc, char *path );
int stor_applefile( SNET *sn, char *pathdesc, char *path, off_t transize,
    char *trancksum, struct applefileinfo *afinfo );
int stor_response( SNET *sn, int *respcount, struct timeval * );
void v_logger( char *string);
int check_capability( char *type, char **capa );
