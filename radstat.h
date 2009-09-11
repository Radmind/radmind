/*
 * Copyright (c) 2008 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */
#ifndef RADSTAT_H
#define RADSTAT_H

#include <sys/stat.h>

#ifdef ENABLE_XATTR
#include "xattr.h"
#endif /* ENABLE_XATTR */

struct radstat {
    struct stat			rs_stat;
    char			rs_type;
    struct applefileinfo	rs_afinfo;

#ifdef ENABLE_XATTR
    struct xattrlist		rs_xlist;
    char			*rs_xname;
#endif /* ENABLE_XATTR */

#ifdef HAVE_SYS_ACL_H
#endif /* HAVE_SYS_ACL_H */
};

int radstat( char *path, struct radstat *rs );

#endif /* RADSTAT_H */
