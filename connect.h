/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

SNET * connectsn( char *host, int port );
int closesn( SNET *sn );

int retr( SNET *sn, char *pathdesc, char *path, char *temppath,
    ssize_t transize, char *trancksum );
int retr_applefile( SNET *sn, char *pathdesc, char *path, char *temppath,
    ssize_t transize, char *trancksum );

int n_stor_file( SNET *sn, char *pathdesc, char *path );
int stor_file( SNET *sn, char *pathdesc, char *path, size_t transize,
    char *trancksum );
int stor_applefile( SNET *sn, char *pathdesc, char *path, size_t transize,
    char *trancksum, struct applefileinfo *afinfo );
