/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

ssize_t do_cksum( char *path, char *cksum_b64 );
ssize_t do_acksum( char *path, char *cksum_b64, struct applefileinfo *afinfo );
