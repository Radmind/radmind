/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

off_t do_fcksum( int fd, char *cksum_b64 );
off_t do_cksum( char *path, char *cksum_b64 );
off_t do_acksum( char *path, char *cksum_b64, struct applefileinfo *afinfo );
