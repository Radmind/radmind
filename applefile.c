/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"
#include "applefile.h"
#include <arpa/inet.h>
#include <stdio.h>

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

/* endian handlers for x86 Macs */
    void
as_entry_netswap( struct as_entry *e )
{
#ifdef __BIG_ENDIAN__
    return;
#else /* __BIG_ENDIAN__ */
     e->ae_id = htonl( e->ae_id );
     e->ae_offset = htonl( e->ae_offset );
     e->ae_length = htonl( e->ae_length );
#endif /* __BIG_ENDIAN__ */
}

    void
as_entry_hostswap( struct as_entry *e )
{
#ifdef __BIG_ENDIAN__
    return;
#else /* __BIG_ENDIAN__ */
     e->ae_id = ntohl( e->ae_id );
     e->ae_offset = ntohl( e->ae_offset );
     e->ae_length = ntohl( e->ae_length );
#endif /* __BIG_ENDIAN__ */
}
