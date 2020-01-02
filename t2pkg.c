#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

#include <openssl/evp.h>

#ifdef __APPLE__
#include <sys/attr.h>
#include <sys/paths.h>
#endif /* __APPLE */

#ifdef linux
#include <sys/vfs.h>
#endif /* linux */

#include "openssl_compat.h" // Compatibility shims for OpenSSL < 1.1.0
#include "applefile.h"
#include "base64.h"
#include "transcript.h"
#include "code.h"
#include "mkdirs.h"
#include "rmdirs.h"
#include "pathcmp.h"
#include "progress.h"
#include "list.h"

extern int	errno;
extern int	showprogress;
extern off_t	lsize;
extern char	*version;

int		tran_format = -1;
int		cksum = 0;
int		force = 0;
int		case_sensitive = 1;
int		verbose = 0;
const EVP_MD    *md;

void            (*logger)( char * ) = NULL;

int copy_file( struct transcript *t, char *dst, char *src, int where );
int local_update( struct transcript *t, char *dst, char *src, int where );
off_t fs_available_space( char *dstdir );

    int
copy_file( struct transcript *t, char *dst, char *src, int where )
{
    struct stat		st;
    int			rfd, wfd, afd = -1, rsrcfd = -1;
    int			type = t->t_pinfo.pi_type;
    unsigned int	md_len;
    char		buf[ 8192 ];
    char		*trancksum = t->t_pinfo.pi_cksum_b64;
    char		*path = t->t_pinfo.pi_name;
    ssize_t		rr, size = 0;
    EVP_MD_CTX          *mdctx = EVP_MD_CTX_new();
    unsigned char       md_value[ SZ_BASE64_D( SZ_BASE64_E( EVP_MAX_MD_SIZE ) ) ];
    char       	        cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
#ifdef __APPLE__
    extern struct as_header as_header;
    extern struct attrlist  getalist;
    extern struct attrlist  setalist;
    struct as_header	header;
    struct as_entry	as_ents[ 3 ];
    struct attr_info	ai;
    char		nullbuf[ FINFOLEN ] = { 0 };
    char		rsrcpath[ MAXPATHLEN ], srcrsrc[ MAXPATHLEN ];
    char		finfo[ 32 ];
    ssize_t		rsize;
    unsigned int	rsrc_len;
#endif /* __APPLE__ */

    if ( cksum ) {
	if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: no checksum\n", t->t_linenum );
	    return( -1 );
	}
	EVP_DigestInit( mdctx, md );
    }

    if (( rfd = open( src, O_RDONLY, 0 )) < 0 ) {
	perror( src );
	return( -1 );
    }
    if (( wfd = open( dst, O_CREAT | O_WRONLY | O_EXCL, 0666 )) < 0 ) {
	perror( dst );
	goto error2;
    }
    if ( fstat( rfd, &st ) < 0 ) {
	perror( src );
	goto error2;
    }

#ifdef __APPLE__
    if ( snprintf( rsrcpath, MAXPATHLEN, "%s%s", dst, _PATH_RSRCFORKSPEC )
		>= MAXPATHLEN ) {
	fprintf( stderr, "%s%s: path too long.\n", dst, _PATH_RSRCFORKSPEC );
	return( -1 );
    } 

    if ( where == K_CLIENT && type == 'a' ) {
	if ( getattrlist( src, &getalist, &ai, sizeof( struct attr_info ),
		FSOPT_NOFOLLOW ) != 0 ) {
	    fprintf( stderr, "getattrlist %s failed.\n", src );
	    goto error2;
	}

	/* add applefile sizes so size in transcript matches */
	st.st_size += ( FINFOLEN + ai.ai_rsrc_len
			+ ( 3 * sizeof( struct as_entry )) + AS_HEADERLEN );
    }
#endif /* __APPLE__ */

    /* check size against transcript */
    if ( st.st_size != t->t_pinfo.pi_stat.st_size ) {
	if ( force ) {
	    fprintf( stderr, "warning: " );
	}
	fprintf( stderr,
		"line %d: size in transcript does not match size of file.\n",
		t->t_linenum );
	if ( !force ) {
	    goto error2;
	}
    }
    size = st.st_size;

#ifdef __APPLE__
    if ( where == K_SERVER && type == 'a' ) {
	/* decode AppleSingle file to filesystem */
	/* XXX What if fs isn't HFS+? */

	/* read AppleSingle header */
	rr = read( rfd, &header, AS_HEADERLEN );
	if ( rr < 0 ) {
	    perror( dst );
	    exit( 2 );
	}
	if ( rr != AS_HEADERLEN ||
		memcmp( &as_header, &header, AS_HEADERLEN ) != 0 ) {
	    fprintf( stderr, "%s: invalid AppleSingle file.\n", src );
	    goto error2;
	}
	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, ( char * )&header, ( unsigned int )rr );
	}
	size -= rr;
	if ( showprogress ) {
	    progressupdate( rr, path );
	}

	/* read the entries */
	rr = read( rfd, ( char * )&as_ents, ( 3 * sizeof( struct as_entry )));
	if ( rr < 0 ) {
	    perror( dst );
	    goto error2;
	}
	if ( rr != ( 3 * sizeof( struct as_entry ))) {
	    fprintf( stderr, "%s: invalid AppleSingle file.\n", src );
	    goto error2;
	}

	/* endian handling, swap header entries if necessary */
	rsrc_len = as_ents[ AS_RFE ].ae_length;
	as_entry_netswap( &as_ents[ AS_FIE ] );
	as_entry_netswap( &as_ents[ AS_RFE ] );
	as_entry_netswap( &as_ents[ AS_DFE ] );

	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, ( char * )&as_ents, ( unsigned int )rr );
	}
	size -= rr;
	if ( showprogress ) {
	    progressupdate( rr, path );
	}

	if ( as_ents[ AS_FIE ].ae_id != ASEID_FINFO ||
		as_ents[ AS_RFE ].ae_id != ASEID_RFORK ||
		as_ents[ AS_DFE ].ae_id != ASEID_DFORK ) {
	    fprintf( stderr, "%s: invalid AppleSingle file.\n", src );
	    goto error2;
	}

	if (( rr = read( rfd, finfo, FINFOLEN )) < 0 ) {
	    perror( dst );
	    goto error2;
	}
	if ( rr != FINFOLEN ) {
	    fprintf( stderr, "%s: invalid AppleSingle file.\n", src );
	    goto error2;
	}
	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, finfo, ( unsigned int )rr );
	}
	if ( showprogress ) {
	    progressupdate( rr, path );
	}

	/* finder info */
        if ( memcmp( finfo, nullbuf, FINFOLEN ) != 0 ) {
            if ( setattrlist( dst, &setalist, finfo, FINFOLEN,
                    FSOPT_NOFOLLOW ) != 0 ) {
                fprintf( stderr, "setattrlist %s failed.\n", dst );
                goto error2;
            }
        }
	size -= rr;

	/* read and write rsrc fork from AppleSingle */
	if (( rsrcfd = open( rsrcpath, O_WRONLY, 0 )) < 0 ) {
	    perror( rsrcpath );
	    goto error2;
	}
	for ( rsize = as_ents[ AS_RFE ].ae_length; rsize > 0; rsize -= rr ) {
	    if (( rr = read( rfd, buf, MIN( sizeof( buf ), rsize ))) <= 0 ) {
		fprintf( stderr, "%s: corrupt AppleSingle file.\n", dst );
		goto error2;
	    }
	    if ( write( rsrcfd, buf, rr ) != rr ) {
		perror( rsrcpath );
		goto error2;
	    }
	    if ( cksum ) {
		EVP_DigestUpdate( mdctx, buf, ( unsigned int )rr );
	    }
	    if ( showprogress ) {
		progressupdate( rr, path );
	    }
	}
	size -= as_ents[ AS_RFE ].ae_length;

	if ( close( rsrcfd ) < 0 ) {
	    perror( dst );
	    goto error2;
	}
    } else if ( where == K_CLIENT && type == 'a' ) {
	/* 
	 * We have to fake the checksum here, since we're copying
	 * from the client. Add the AppleSingle header and faked
	 * entries to the checksum digest.
	 */
	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, &as_header, AS_HEADERLEN );

	    as_ents[AS_FIE].ae_id = ASEID_FINFO;
	    as_ents[AS_FIE].ae_offset = AS_HEADERLEN +
			( 3 * sizeof( struct as_entry ));
	    as_ents[AS_FIE].ae_length = FINFOLEN;
	    
	    as_ents[AS_RFE].ae_id = ASEID_RFORK;
	    as_ents[AS_RFE].ae_offset = ( as_ents[AS_FIE].ae_offset
					+ as_ents[AS_FIE].ae_length );
	    as_ents[AS_RFE].ae_length = ai.ai_rsrc_len;

	    as_ents[AS_DFE].ae_id = ASEID_DFORK;
	    as_ents[AS_DFE].ae_offset = ( as_ents[AS_RFE].ae_offset
					+ as_ents[AS_RFE].ae_length );
	    as_ents[AS_DFE].ae_length = ( st.st_size -
					    ( AS_HEADERLEN +
					    ( 3 * sizeof( struct as_entry )) +
					    FINFOLEN + ai.ai_rsrc_len  ));

	    /* endian handling, swap header entries if necessary */
	    rsrc_len = as_ents[ AS_RFE ].ae_length;
	    as_entry_netswap( &as_ents[ AS_FIE ] );
	    as_entry_netswap( &as_ents[ AS_RFE ] );
	    as_entry_netswap( &as_ents[ AS_DFE ] );

	    EVP_DigestUpdate( mdctx, ( char * )&as_ents,
					    ( 3 * sizeof( struct as_entry )));
	} 
	size -= ( AS_HEADERLEN + ( 3 * sizeof( struct as_entry )) + FINFOLEN );
	if ( showprogress ) {
	    progressupdate(( AS_HEADERLEN + FINFOLEN +
				( 3 * sizeof( struct as_entry ))), path );
	} 

	/* finder info */
	if ( memcmp( ai.ai_data, nullbuf, FINFOLEN ) != 0 ) {
	    if ( setattrlist( dst, &setalist, ai.ai_data, FINFOLEN,
		    FSOPT_NOFOLLOW ) != 0 ) {
		fprintf( stderr, "setattrlist %s failed.\n", dst );
		goto error1;
	    }
	}
	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, ai.ai_data, FINFOLEN );
	}

	/* read and write the finder info and rsrc fork from the system */
	if ( ai.ai_rsrc_len > 0 ) {
	    if ( snprintf( srcrsrc, MAXPATHLEN, "%s%s",
		    src, _PATH_RSRCFORKSPEC ) >= MAXPATHLEN ) {
		fprintf( stderr, "%s%s: path too long\n",
			dst, _PATH_RSRCFORKSPEC );
		return( -1 );
	    }
	    
	    if (( afd = open( srcrsrc, O_RDONLY, 0 )) < 0 ) {
		perror( srcrsrc );
		return( -1 );
	    }
	    if (( rsrcfd = open( rsrcpath, O_WRONLY, 0 )) < 0 ) {
		perror( rsrcpath );
		goto error2;
	    }
	    while (( rr = read( afd, buf, sizeof( buf ))) > 0 ) {
		if ( write( rsrcfd, buf, rr ) != rr ) {
		    perror( rsrcpath );
		    goto error2;
		}
		if ( cksum ) {
		    EVP_DigestUpdate( mdctx, buf, rr );
		}
		size -= rr;
		if ( showprogress ) {
		    progressupdate( rr, path );
		}
	    }
	    if ( rr < 0 ) {
		perror( srcrsrc );
		goto error2;
	    }	
	    
	    if ( close( afd ) < 0 ) {
		perror( srcrsrc );
		goto error2;
	    }
	    if ( close( rsrcfd ) < 0 ) {
		perror( rsrcpath );
		goto error1;
	    }
	}
    }
#else /* __APPLE__ */
    if ( type == 'a' ) {
	fprintf( stderr, "decode applefile %s invalid\n", path );
	goto error2;
    }
#endif /* __APPLE */
	
    /* write the data fork */
    while (( rr = read( rfd, buf, sizeof( buf ))) > 0 ) {
	if ( write( wfd, buf, rr ) != rr ) {
	    perror( "write" );
	    goto error2;
	}
	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, buf, rr );
	}
	size -= rr;
	if ( showprogress ) {
	    progressupdate( rr, path );
	}
    }
    if ( rr < 0 ) {
	perror( dst );
	exit( 2 );
    }

    if ( close( rfd ) < 0 ) {
	perror( dst );
	goto error2;
    }
    if ( close( wfd ) < 0 ) {
	perror( dst );
	goto error1;
    }

    if ( size != 0 ) {
	fprintf( stderr, "line %d: copied wrong number of bytes.\n",
		t->t_linenum );
	fprintf( stderr, "FILE %s\n", path );
	goto error1;
    }

    if ( cksum ) {
	EVP_DigestFinal( mdctx, md_value, &md_len );
	base64_e( md_value, md_len, ( char * )cksum_b64 );
        EVP_MD_CTX_free(mdctx);
	if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    if ( force ) {
		fprintf( stderr, "warning: " );
	    }
	    fprintf( stderr, "line %d: checksum mismatch\n", t->t_linenum );
	    if ( !force ) {
		goto error1;
	    }
	}
    }

    return( 0 );

error2:
    close( rfd );
    close( wfd );
    if ( afd >= 0 ) {
	close( afd );

    }
    if ( rsrcfd >= 0 ) {
	close( rsrcfd );
    }

error1:
    return( -1 );
}

    int
local_update( struct transcript *t, char *dst, char *src, int where )
{
    int			type = t->t_pinfo.pi_type;
    mode_t		mode;
    uid_t		owner;
    gid_t		group;
    struct utimbuf	times;
#ifdef __APPLE__
    extern struct attrlist	setalist;
    static char			nullbuf[ FINFOLEN ] = { 0 };
#endif /* __APPLE__ */

    switch ( type ) {
    case 'a':
    case 'f':
	if ( copy_file( t, dst, src, where ) != 0 ) {
	    return( 1 );
	}
	break;

    case 'd':
	mode = t->t_pinfo.pi_stat.st_mode;
	if ( mkdir( dst, mode ) < 0 ) {
	    if ( errno != EEXIST ) {
		perror( dst );
		return( 1 );
	    }
	}

#ifdef __APPLE__
	/* set finder info, if necessary */
	if ( memcmp( t->t_pinfo.pi_afinfo.ai.ai_data,
				nullbuf, FINFOLEN ) != 0 ) {
	    if ( setattrlist( dst, &setalist,
			t->t_pinfo.pi_afinfo.ai.ai_data, FINFOLEN,
			FSOPT_NOFOLLOW ) != 0 ) {
		fprintf( stderr, "setattrlist for %s failed.\n", dst );
		return( 1 );
	    }
	}
#endif /* __APPLE__ */
	break;

    case 'h':
	if ( link( t->t_pinfo.pi_link, dst ) != 0 ) {
	    perror( dst );
	    return( 1 );
	}
	goto done;

    case 'l':
	if ( symlink( t->t_pinfo.pi_link, dst ) != 0 ) {
	    perror( dst );
	    return( 1 );
	}
	goto done;

    case 'p':
	mode = t->t_pinfo.pi_stat.st_mode | S_IFIFO;

	if ( mkfifo( dst, mode ) != 0 ) {
	    perror( dst );
	    return( 1 );
	}
	break;

    case 'b':
    case 'c':
	mode = t->t_pinfo.pi_stat.st_mode;
	if ( type == 'b' ) {
	    mode |= S_IFBLK;
	} else {
	    mode |= S_IFCHR;
	}

	if ( mknod( dst, mode, t->t_pinfo.pi_stat.st_rdev ) != 0 ) {
	    perror( dst );
	    return( 1 );
	}
	break;

    case 'D':	/* can't actually do anything with these */
    case 's':
	fprintf( stderr, "Warning: line %d: %c %s not created...continuing\n",
		t->t_linenum, type, dst );
	goto done;

    default:
	fprintf( stderr, "%c: unknown object type\n", type );
	return( 1 );
    }

    owner = t->t_pinfo.pi_stat.st_uid;
    group = t->t_pinfo.pi_stat.st_gid;
    if ( chown( dst, owner, group ) != 0 ) {
	perror( dst );
	return( 1 );
    }
    mode = ( T_MODE & t->t_pinfo.pi_stat.st_mode );
    if ( chmod( dst, mode ) != 0 ) {
	perror( dst );
	return( 1 );
    }

    if ( type == 'a' || type == 'f' ) {
	times.modtime = t->t_pinfo.pi_stat.st_mtime;
	times.actime = t->t_pinfo.pi_stat.st_mtime;
	if ( utime( dst, &times ) != 0 ) {
	    perror( dst );
	    return( 1 );
	}
    }

done:
    return( 0 );
}

    off_t
fs_available_space( char *dstdir )
{
    struct statfs	s;
    off_t		block_size, free_blocks;

    if ( statfs( dstdir, &s ) != 0 ) {
	fprintf( stderr, "statfs %s: %s\n", dstdir, strerror( errno ));
	return( -1 );
    }
    block_size = s.f_bsize;
    free_blocks = s.f_bfree;

    return( block_size * free_blocks );
}

    int
main( int ac, char *av[] )
{
    extern char		*optarg;
    extern int		optind;
    struct transcript	*t;
    struct stat		st;
    char		*transcript;
    char		shortname[ MAXPATHLEN ], fullpath[ MAXPATHLEN ];
    char		src[ MAXPATHLEN ], dst[ MAXPATHLEN ];
    char		root[ MAXPATHLEN ] = "/";
    char		*kfile = "/dev/null";
    char		*p, *dstdir = NULL;
    char		*radmind_path = _RADMIND_PATH;
    int			c, err = 0;
    int			where = K_CLIENT;
    off_t		space;
    FILE		*f;

    while (( c = getopt( ac, av, "%c:D:d:FIr:sV" )) != EOF ) {
	switch ( c ) {
	case '%':	/* % done progress */
	    showprogress = 1;
	    break;
	    
	case 'c':	/* cksum */
	    OpenSSL_add_all_digests();
	    md = EVP_get_digestbyname( optarg );
	    if ( !md ) {
		fprintf( stderr, "%s: unsupported checksum\n", optarg );
		exit( 2 );
	    }
	    cksum = 1;
	    break;

	case 'D':	/* radmind directory */
	    radmind_path = optarg;
	    break;

	case 'd':	/* where to copy files */
	    dstdir = optarg;
	    break;

	case 'F':	/* force if size or checksum mismatch */
	    force = 1;
	    break;

	case 'I':	/* case-insensitive transcripts */
	    case_sensitive = 0;
	    break;

	case 'r':	/* root path */
	    if ( strlen( optarg ) >= MAXPATHLEN ) {
		fprintf( stderr, "%s: path too long\n", optarg );
		exit( 2 );
	    }
	    strcpy( root, optarg );
	    if ( stat( root, &st ) < 0 ) {
		perror( root );
		exit( 2 );
	    }
	    break;

	case 's':	/* running on server */
	    where = K_SERVER;
	    break;

	case 'V':	/* version */
	    printf( "%s\n", version );
	    exit( 0 );

	default:
	    err++;
	    break;
	}
    }
	
    if ( err || dstdir == NULL || ( ac - optind != 1 )) {
	fprintf( stderr, "Usage: %s { -d dstdir } [ -Fs ] [ -c cksum ] "
		"[ -D radmind_path ]  [ -r root_path ] "
		"transcript.T\n", av[ 0 ] );
	exit( 1 );
    }

    transcript = av[ optind ];

    if ( realpath( transcript, fullpath ) == NULL ) {
	fprintf( stderr, "realpath %s: %s\n", transcript, strerror( errno ));
	exit( 2 );
    }
    if (( p = strrchr( transcript, '/' )) == NULL ) {
	if ( strlen( transcript ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s: path too long\n", transcript );
	    exit( 2 );
	}
	strcpy( shortname, transcript );
    } else {
	p++;
	if ( strlen( p ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s: path too long\n", p );
	    exit( 2 );
	}
	strcpy( shortname, p );
    }

    if ( mkdirs( dstdir ) < 0 ) {
	fprintf( stderr, "mkdirs %s: %s\n", dstdir, strerror( errno ));
	exit( 2 );
    }

    if ( stat( root, &st ) != 0 ) {
	fprintf( stderr, "stat %s: %s\n", root, strerror( errno ));
	exit( 2 );
    }
    if ( mkdir( dstdir, 0777 ) != 0 ) {
	if ( errno != EEXIST ) {
	    perror( dstdir );
	    exit( 2 );
	}
    }
    if ( chmod( dstdir, st.st_mode ) != 0 ) {
	perror( "chmod" );
	exit( 2 );
    }
    if ( chown( dstdir, st.st_uid, st.st_gid ) != 0 ) {
	perror( "chown" );
	exit( 2 );
    }

    /* make sure we've got enough space for copying */
    if (( f = fopen( transcript, "r" )) == NULL ) {
	perror( transcript );
	exit( 2 );
    }
    lsize = loadsetsize( f );
    if ( fclose( f ) != 0 ) {
	fprintf( stderr, "fclose %s: %s", transcript, strerror( errno ));
	exit( 2 );
    }
    if (( space = fs_available_space( dstdir )) < 0 ) {
	exit( 2 );
    }
    if ( lsize >= space ) {
	fprintf( stderr, "%s: insufficient disk space for %s.\n",
		dstdir, shortname );
	exit( 1 );
    }

    if ( where == K_SERVER ) {
	if ( snprintf( root, MAXPATHLEN, "%s/file/%s",
			radmind_path, shortname ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s/file/%s: path too long\n",
			radmind_path, shortname );
	    exit( 2 );
	}
    }

    outtran = stdout;
    edit_path = CREATABLE;
    transcript_init( kfile, where );

    t_new( T_POSITIVE, fullpath, shortname, kfile );

    for ( ;; ) {
	t = transcript_select();
	
	if ( t->t_eof ) {
	    break;
	}

	/* create the destination path */
	if ( snprintf( dst, MAXPATHLEN, "%s/%s", dstdir, t->t_pinfo.pi_name )
		>= MAXPATHLEN ) {
	    fprintf( stderr, "%s/%s: path too long\n", dstdir,
			t->t_pinfo.pi_name );
	    exit( 2 );
	}
	if ( mkdirs( dst ) < 0 ) {
	    fprintf( stderr, "mkdirs %s: %s\n", dst, strerror( errno ));
	}

	/* and the source path */
	if ( snprintf( src, MAXPATHLEN, "%s/%s", root, t->t_pinfo.pi_name )
		>= MAXPATHLEN ) {
	    fprintf( stderr, "%s/%s: path too long\n", dstdir,
			t->t_pinfo.pi_name );
	    exit( 2 );
	}

	if ( local_update( t, dst, src, where ) != 0 ) {
	    /* XXX is this really a good idea? */
	    if ( rmdirs( dstdir ) != 0 ) {
		fprintf( stderr, "rmdirs failed, delete %s manually.\n",
			dstdir );
		exit( 2 );
	    }
	}

	transcript_parse( t );
    }

    transcript_free();

    return( 0 );
}
