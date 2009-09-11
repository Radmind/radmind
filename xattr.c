#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xattr.h"

#ifdef ENABLE_XATTR
#ifdef HAVE_SYS_XATTR_H
/*
 * xattr_list: return the size of the extended attribute names
 * for path. if xlist is non-null, the xattr name list will be
 * copied into it. the result must be freed by the caller.
 */
#ifdef __APPLE__
    ssize_t
xattr_list( char *path, char **xlist )
{
    ssize_t		len;
    char		*list;

    /* TEMPORARY */
    if ( xlist == NULL ) {
	return( 0 );
    }

    len = listxattr( path, NULL, 0, XATTR_NOFOLLOW );
    if ( len == 0 || len == -1 ) {
	*xlist = NULL;
	return( len );
    }

    if (( list = ( char * )malloc( len )) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    if (( len = listxattr( path, list, len, XATTR_NOFOLLOW )) < 0 ) {
	free( list );
	list = NULL;
    }

    *xlist = list;

    return( len );
}

/*
 * xattr_get: get the contents of an xattr. returns the size of the attribute
 * xname or -1 on error. xbuf contains the xattr contents or a portion of them.
 */
    ssize_t
xattr_get( char *path, char *xname, char **xbuf, unsigned int offset )
{
    ssize_t		xlen;
    static char		*buf = NULL;

    if ( buf == NULL ) {
	if (( buf = malloc( RADMIND_XATTR_BUF_SZ )) == NULL ) {
	    return( -1 );
	}
    }

    /*
     * Apple gives us an offset, which allows looped reads of xattrs,
     * but only for xattrs called com.apple.ResourceFork.
     *
     * And does the performance really suck? Is it equivalent to an
     * open+lseek+read+close for every pass of the loop?
     */
    errno = 0;
    xlen = getxattr( path, xname, buf, RADMIND_XATTR_BUF_SZ,
			offset, XATTR_NOFOLLOW );
    if ( errno == ERANGE ) {
	/* buf is too small. try to get the whole thing. */
	if (( xlen = getxattr( path, xname, NULL, 0, 0, XATTR_NOFOLLOW )) < 0) {
	    return( -1 );
	}
	if (( buf = realloc( buf, xlen )) == NULL ) {
	    return( -1 );
	}
	xlen = getxattr( path, xname, buf, xlen,
			offset, XATTR_NOFOLLOW );
    }

    *xbuf = buf;

    return( xlen );
}

    int
xattr_set( char *path, char *name, char *value,
		size_t size, unsigned int offset )
{
    return( setxattr( path, name, value, size, offset, XATTR_NOFOLLOW ));
}

    int
xattr_remove( char *path, char *name )
{
    return( removexattr( path, name, XATTR_NOFOLLOW ));
}

#elif defined( HAVE_LGETXATTR )	/* presumably Linux */

    ssize_t
xattr_list( char *path, char **xlist )
{
    ssize_t		len;
    char		*list;

    /* TEMPORARY */
    if ( xlist == NULL ) {
	return( 0 );
    }

    len = llistxattr( path, NULL, 0 );
    if ( len == 0 || len == -1 ) {
	*xlist = NULL;
	return( len );
    }

    if (( list = ( char * )malloc( len )) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    if (( len = llistxattr( path, list, len )) < 0 ) {
	free( list );
	list = NULL;
    }

    *xlist = list;

    return( len );
}

/*
 * xattr_get: get the contents of an xattr. returns the size of the attribute
 * xname or -1 on error. xbuf contains the xattr contents or a portion of them.
 */
    ssize_t
xattr_get( char *path, char *xname, char **xbuf, unsigned int offset )
{
    ssize_t		xlen;
    static char		*buf = NULL;

    if ( buf == NULL ) {
	if (( buf = malloc( RADMIND_XATTR_BUF_SZ )) == NULL ) {
	    return( -1 );
	}
    }

    errno = 0;
    xlen = lgetxattr( path, xname, buf, RADMIND_XATTR_BUF_SZ );
    if ( errno == ERANGE ) {
	/* buf is too small. try to get the whole thing. */
	if (( buf = realloc( buf, xlen )) == NULL ) {
	    return( -1 );
	}
	xlen = lgetxattr( path, xname, buf, xlen );
    }

    *xbuf = buf;

    return( xlen );
}

    int
xattr_set( char *path, char *name, char *value,
		size_t size, unsigned int offset )
{
    return( lsetxattr( path, name, value, size, 0 ));
}

    int
xattr_remove( char *path, char *name )
{
    return( lremovexattr( path, name ));
}

#endif /* __APPLE__ || HAVE_LGETXATTR */

#elif defined( HAVE_SYS_EXTATTR_H )

/* this is presumably FreeBSD */

#endif /* HAVE_SYS_XATTR_H || HAVE_SYS_EXTATTR_H */

#endif /* ENABLE_XATTR */

/* return the real path to the file for the given fake xattr path */
    char *
xattr_get_path( char *xpath )
{
    int			plen;
    static char		path[ MAXPATHLEN ];
    char		*p;

    if (( p = strrchr( xpath, '/' )) == NULL ) {
	/*
	 * by design, an xattr path *must* contain a slash separating
	 * parent file from xattr name. no slash is a bad path.
	 */
	errno = EINVAL;
	return( NULL );
    }

    plen = p - xpath;
    if ( plen <= 0 || plen >= MAXPATHLEN ) {
	errno = ENAMETOOLONG;
	return( NULL );
    }
    
    strncpy( path, xpath, plen );
    path[ plen ] = '\0';

    return( path );
}

/*
 * return the xattr name for a given fake xattr path. if xbuf
 * is not NULL, it is presumed to be a buffer of size MAXPATHLEN and
 * the xattr name will be copied into it, and the call will return
 * xbuf. if xbuf is NULL, the call uses the internal static buffer.
 */
    char *
xattr_get_name( char *xpath, char *xbuf )
{
    int			xlen;
    static char		xname[ MAXPATHLEN ];
    char		*xret = NULL;
    char		*p;

    if (( p = strrchr( xpath, '/' )) == NULL ) {
	/* must have a slash separating parent file from xattr name. */
	errno = EINVAL;
	return( NULL );
    }

    p++;
    if ( *p == '\0' ) {
	/* presumably a path like '/path/to/file/' */
	errno = EINVAL;
	return( NULL );
    }

    xlen = strlen( xpath ) - ( p - xpath ) - strlen( RADMIND_XATTR_XTN );
    if ( xlen <= 0 || xlen >= MAXPATHLEN ) {
	errno = ENAMETOOLONG;
	return( NULL );
    }

    if ( xbuf != NULL ) {
	/* use buffer passed in */
	strncpy( xbuf, p, xlen );
	xbuf[ xlen ] = '\0';
	xret = xbuf;
    } else {
	strncpy( xname, p, xlen );
	xname[ xlen ] = '\0';
	xret = xname;
    }

    return( xret );
}

/*
 * encode an xattr name. we'd normally just use encode() for this, but
 * xattrs, at least on Mac OS X and Linux, can contain slashes. Since
 * the Solaris implementation relies on files and paths, it's unlikely
 * to permit slashes in filenames. Not sure about FreeBSD yet.
 *
 * slashes are encoded as "\x2f". the backslash is then encoded by
 * encode() from code.c as "\\".
 */
    char *
xattr_name_encode( char *raw )
{
    /* enough space on the stack to hold escaped slashes. */
    static char		enc[ ( RADMIND_XATTR_MAXNAMELEN * 4 ) + 1 ];
    char		*p, *ep;
    
    if ( raw == NULL ) {
	return( NULL );
    }
    if ( strchr( raw, '/' ) == NULL ) {
	return( raw );
    }

    if ( strlen( raw ) >= sizeof( enc )) {
	errno = ENAMETOOLONG;
	return( NULL );
    }

    ep = enc;

    for ( p = raw; *p != '\0'; p++ ) {
	switch ( *p ) {
	case '/' :
	    snprintf( ep, 5, "\\x%02x", *p );
	    ep += 4;
	    break;

	default :
	    *ep = *p;
	    ep++;
	    break;
	}
    }
    *ep = '\0';

    return( enc );
}

    char *
xattr_name_decode( char *enc )
{
    static char		dec[ RADMIND_XATTR_MAXNAMELEN + 1 ];
    char		*p, *dp;

    if ( enc == NULL ) {
	return( NULL );
    }
    if (( p = strchr( enc, '\\' )) == NULL ) {
	return( enc );
    }

    if ( strlen( enc ) >= (( RADMIND_XATTR_MAXNAMELEN * 4 ) + 1 )) {
	errno = ENAMETOOLONG;
	return( NULL );
    }

    dp = dec;

    for ( p = enc; *p != '\0'; p++, dp++ ) {
	if (( dp - dec ) >= sizeof( dec )) {
	    errno = ENAMETOOLONG;
	    return( NULL );
	}

	switch ( *p ) {
	case '\\' :
	    if ( strncmp( p, "\\x2f", strlen( "\\x2f" )) == 0 ) {
		*dp = '/';
		p += 3;		/* loop adds final increment */
		continue;
	    }

	default :
	    *dp = *p;
	    break;
	}
    }
    *dp = '\0';

    return( dec );
}
