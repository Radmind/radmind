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

#define AS_HEADERLEN	26
#define AS_MAGIC	0x00051600
#define AS_VERSION	0x00020000
#define AS_NENTRIES	3

#define ASEID_DFORK	1
#define ASEID_RFORK	2
#define ASEID_FINFO	9

#define AS_FIE		0	/* for array of ae_entry structs */
#define AS_RFE		1
#define AS_DFE		2	

#define FINFOLEN	32

/* applesingle entry */
struct as_entry {
    unsigned long	ae_id;
    unsigned long	ae_offset;
    unsigned long	ae_length;
};

/* applesingle header */
struct as_header {
    unsigned long	ah_magic;
    unsigned long	ah_version;
    unsigned char	ah_filler[ 16 ];
    unsigned short	ah_num_entries;
};

struct finderinfo {
    unsigned long   	fi_size;
    char		fi_data[ FINFOLEN ];
};

struct applefileinfo {
    struct finderinfo	fi;		// finder info
    struct as_entry	as_entry[ 3 ];	// Apple Single entries
					// For Finder info, rcrs and data forks
    size_t		as_size;	// Total size 
};
