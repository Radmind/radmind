/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

/*
 * applesingle format:
 *  header:
 *      -magic number (4 bytes)
 *      -version number (4 bytes)
 *      -filler (16 bytes)
 *      -number of entries (2 bytes)
 *      -x number of entries, with this format:
 *          id (4 bytes)
 *          offset (4 bytes)
 *          length (4 bytes)
 *  data:
 *      -finder info
 *      -rsrc fork
 *      -data fork
 */

#include <sys/types.h>
#include <sys/param.h>
#include <inttypes.h>

#define AS_HEADERLEN	26

#ifdef __BIG_ENDIAN__

#define AS_MAGIC       0x00051600
#define AS_VERSION     0x00020000
#define AS_NENTRIES    0x0003

#else /* __BIG_ENDIAN__ */

#define AS_MAGIC       0x00160500
#define AS_VERSION     0x00000200
#define AS_NENTRIES    0x0300

#endif /* __BIG_ENDIAN__ */

#define ASEID_DFORK    1
#define ASEID_RFORK    2
#define ASEID_FINFO    9

#define AS_FIE		0	/* for array of ae_entry structs */
#define AS_RFE		1
#define AS_DFE		2	

#define FINFOLEN	32

#define FI_CREATOR_OFFSET 4

/* applesingle entry */
struct as_entry {
    uint32_t	ae_id;
    uint32_t	ae_offset;
    uint32_t	ae_length;
};

/* applesingle header */
struct as_header {
    uint32_t	ah_magic;
    uint32_t	ah_version;
    uint8_t	ah_filler[ 16 ];
    uint16_t	ah_num_entries;
};

struct attr_info {
    uint32_t   	ai_size;
    uint8_t	ai_data[ FINFOLEN ];
    off_t 	ai_rsrc_len;
};

struct applefileinfo {
    struct attr_info	ai;		// finder info
    struct as_entry	as_ents[ 3 ];	// Apple Single entries
					// For Finder info, rcrs and data forks
    off_t		as_size;	// Total apple single file size 
};

void as_entry_netswap( struct as_entry *e );
void as_entry_hostswap( struct as_entry *e );
off_t ckapplefile( char *applefile, int afd );
