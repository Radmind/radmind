#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sha.h>
#include <unistd.h>

#include "base64.h"
#include "chksum.h"

    int 
do_chksum( char *path, char *chksum_b64 )
{
    int			fd;

    if (( fd = open( path, O_RDONLY, 0 )) < 0 ) {
	return( -1 );
    }

    if ( do_chksum_fd( fd, chksum_b64 ) != 0 ) {
	return( -1 );
    }

    if ( close( fd ) != 0 ) {
	return( -1 );
    }

    return( 0 );
}

    int
do_chksum_fd( int fd, char *chksum_b64 )
{
    unsigned long	rr;
    unsigned char	buf[ 8192 ];
    unsigned char	md[ SHA_DIGEST_LENGTH ];
    unsigned char	mde[ SZ_BASE64_E( sizeof( md )) ];
    SHA_CTX		sha_ctx;

    SHA1_Init( &sha_ctx );

    while (( rr = read( fd, buf, sizeof( buf ))) > 0 ) {
	SHA1_Update( &sha_ctx, buf, rr );
    }

    if ( rr < 0 ) {
	return( -1 );
    }

    SHA1_Final( md, &sha_ctx );

    base64_e( md, sizeof( md ), mde );
    strcpy( chksum_b64, mde );

    return( 0 );
}
