#define AS_HEADERLEN	26
#define AS_BUFLEN	8192
#define AS_MAGIC	0x00051600
#define AS_VERSION	0x00020000
#define NUM_ENTRIES	3

#define ASEID_DFORK	1
#define ASEID_RFORK	2
#define ASEID_FINFO	9

struct as_entry {
    unsigned long	ae_id;
    unsigned long	ae_offset;
    unsigned long	ae_length;
};

struct as_header {
    unsigned long	ah_magic;
    unsigned long	ah_version;
    unsigned char	ah_filler[ 16 ];
    unsigned short	ah_num_entries;
};

int decode_asingle_file( const char *path );
int chk_for_finfo( const char *path, char *finfo );
