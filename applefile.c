/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"
#include "applefile.h"

#ifdef __APPLE__
#include <sys/attr.h>

struct attrlist		getalist = {
    ATTR_BIT_MAP_COUNT,
    0,
    ATTR_CMN_FNDRINFO,
    0,
    0,
    ATTR_FILE_RSRCLENGTH,
    0,
};

struct attrlist		getdiralist = {
    ATTR_BIT_MAP_COUNT,
    0,
    ATTR_CMN_FNDRINFO,
    0,
    0,
    0,
    0,
};

struct attrlist		setalist = {
    ATTR_BIT_MAP_COUNT,
    0,
    ATTR_CMN_FNDRINFO,
    0,
    0,
    0,
    0,
};
#endif /* __APPLE__ */

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
