/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef __APPLE__
#include <sys/attr.h>

#include "applefile.h"

struct attrlist		alist = {
    ATTR_BIT_MAP_COUNT,
    0,
    ATTR_CMN_FNDRINFO,
    0,
    0,
    0,
    0,
};

struct as_header		as_header = {
    AS_MAGIC,
    AS_VERSION,
    {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    AS_NENTRIES,
};

extern struct as_header as_header;
#endif /* __APPLE__ */
