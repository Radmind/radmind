/* check file for AS magic number. If AS, decode. */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/paths.h>
#include <sys/attr.h>
#include <string.h>
#include <snet.h>

#include "afile.h"
#include "bprint.h"

extern struct timeval	timeout;

struct as_header	as_header = {
    0x00051600,
    0x00020000,
    {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    NUM_ENTRIES,
};

    int
decode_asingle_file( const char *path )
{
    int			rc;
    int			err = 0, asfd, ofd, rsrcfd, as_cc;
    unsigned char	finfo[ 32 ];
    char		*dec_name;
    char		as_buf[ AS_BUFLEN ];
    char		*rsrc_path;
    const char		*rsrc_suffix = _PATH_RSRCFORKSPEC;
    struct stat 	*as_stp;
    struct attrlist	al;
    struct as_header	as_dest;
    struct as_entry	ae_finfo;
    struct as_entry	ae_rfork;
    struct as_entry	ae_dfork;

    memset( &al, 0, sizeof( al ));
    al.bitmapcount = ATTR_BIT_MAP_COUNT;
    al.commonattr = ATTR_CMN_FNDRINFO;

    dec_name = strtok( path, "." );

    memset( &as_dest, '\0', ( int )sizeof( struct as_header ));
    memset( &ae_finfo, '\0', ( int )sizeof( struct as_entry ));
    memset( &ae_rfork, '\0', ( int )sizeof( struct as_entry ));
    memset( &ae_dfork, '\0', ( int )sizeof( struct as_entry ));

    if (( asfd = open( path, O_RDONLY )) < 0 ) {
	perror( path );
	exit( 1 );
    }

    if (( as_stp = ( void * )malloc(( int )sizeof( struct stat ))) == NULL ) {
	perror( "malloc" );
	exit( 1 );
    }

    /* read header to determine if file is encoded in applesingle */
    if (( as_cc = read( asfd, &as_dest, AS_HEADERLEN ))
		< 0 ) {
	perror( "read" ); 
	exit( 1 );
    }

    printf( "%d\n", ( int )sizeof( struct as_header ));
    if ( as_dest.ah_magic != AS_MAGIC 
	    || as_dest.ah_version != AS_VERSION 
	    || as_dest.ah_num_entries != NUM_ENTRIES ) {
	printf( "%s is not a radmind AppleSingle file.\n", path );
	close( asfd );
	free( as_stp );
	exit( 1 );
    }

    /* read finder info header entry */
    if (( as_cc = read( asfd, &ae_finfo, ( int )sizeof( struct as_entry )))
		< 0 ) {
	perror( "asfd read" );
	exit( 1 );
    }

    /* read rsrc fork header entry */
    if (( as_cc = read( asfd, &ae_rfork, ( int )sizeof( struct as_entry )))
		< 0 ) {
	perror( "asfd read" );
	exit( 1 );
    }

    /* read data fork header entry */
    if (( as_cc = read( asfd, &ae_dfork, ( int )sizeof( struct as_entry )))
		< 0 ) {
	perror( "asfd read" );
	exit( 1 );
    }

    if (( ofd = open( dec_name, O_CREAT | O_EXCL | O_WRONLY, 0666 )) < 0 ) {
	perror( dec_name );
	exit( 1 );
    }

    if (( as_cc = read( asfd, finfo, sizeof( finfo ))) < 0 ) {
	perror( "asfd read finder info" );
	exit( 1 );
    }

    printf( "%d bytes read from finder info.\n", as_cc );

    if (( rsrc_path = ( char * )malloc( strlen( path ) + 23 )) == NULL ) {
        perror( "malloc" );
        exit( 1 );
    }

    snprintf( rsrc_path, MAXPATHLEN, "%s%s", dec_name, rsrc_suffix );
        
    printf( "path to rsrc fork:\n\t%s\n", rsrc_path );

    if (( rsrcfd = open( rsrc_path, O_WRONLY, 0 )) < 0 ) {
        perror( rsrc_path );
        exit( 1 );
    };  

    if (( as_cc = read( asfd, as_buf, ae_rfork.ae_length )) < 0 ) {
    	perror( "asfd read" );
	exit( 1 );
    }

    if (( write( rsrcfd, as_buf, ( unsigned int )as_cc )) != as_cc ) {
    	perror( "rsrcfd write" );
	exit( 1 );
    }

    if ( close( rsrcfd ) < 0 ) {
	perror( "close rsrcfd" );
	exit( 1 );
    }

    /* write data fork to file */
    while (( as_cc = read( asfd, as_buf, AS_BUFLEN )) > 0 ) {
	if ( write( ofd, as_buf, ( unsigned int )as_cc ) != as_cc ) {
	    perror( "ofd write" );
	    exit( 1 );
	}
    }

    if ( close( ofd ) < 0 ) {
	perror( "close ofd" );
	exit( 1 );
    }

    if ( close( asfd ) < 0 ) {
	perror( "close" );
	exit( 1 );
    }

    if (( rc = setattrlist( dec_name, &al, finfo, 36, FSOPT_NOFOLLOW ))) {
	perror( "setattrlist" );
	exit( 1 );
    }

    free( as_stp );
    free( rsrc_path );

    return( err );
}

/* encode hfs+ file to applesingle format with .as_enc suffix (for now ) */

/*
 * applesingle format:
 *  header:
 *	-magic number (4 bytes)
 *	-version number (4 bytes)
 *	-filler (16 bytes)
 *	-number of entries (2 bytes)
 *	-x number of entries, with this format:
 *	    id (4 bytes)
 *	    offset (4 bytes)
 *	    length (4 bytes)
 *  data:
 *	-finder info
 *	-rsrc fork
 *	-data fork
 */

    int
chk_for_finfo( const char *path, char *finfo )
{
    int			err = 0;
    char		null_buf[ 32 ];
    struct {
	unsigned long	ssize;
	char		finfo_buf[ 32 ];
    } finfo_struct;
    struct attrlist	al;

    memset( &al, 0, sizeof( al ));
    memset( finfo_struct.finfo_buf, 0, sizeof( finfo_struct.finfo_buf ));
    memset( null_buf, 0, sizeof( null_buf ));

    al.bitmapcount = ATTR_BIT_MAP_COUNT;
    al.commonattr = ATTR_CMN_FNDRINFO;

    if (( err = getattrlist( path, &al, &finfo_struct, sizeof( finfo_struct ),
		FSOPT_NOFOLLOW ))) {
	return( err );
    }

    memcpy( finfo, finfo_struct.finfo_buf, sizeof( finfo_struct.finfo_buf ));

    if ( memcmp( finfo, null_buf, sizeof( null_buf )) == 0 ) {
	err++;
    }

    return( err );
}
