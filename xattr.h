/*
 * Convenience wrappers for Extended Attribute support.
 *
 * Apple's implementation of the xattr API differs from most others:
 *
 * 1) getxattr & setxattr can take an offset to indicate where to read
 *    from or write to within the attribute (supports large rsrc forks).
 * 2) instead of implementing lgetxattr, lsetxattr & lremovexattr, Apple
 *    adds a flags parameter to the getxattr, setxattr & removexattr
 *    calls. XATTR_NOFOLLOW tells the calls not to resolve symlinks.
 * 3) xattrs are stored in forks. this means that the xattrs can be
 *    enormous. the current size limitation (as determined by the old
 *    AppleSingle RFC) is probably UINT32_MAX, but could be even larger.
 *    By contrast, according to the Linux xattr manpages, the extNfs
 *    implementation limits xattr sizes to block size. Of course, the
 *    same Linux manpages suggest there's "no practical limit" to xattr
 *    sizes on XFS.
 * 4) Apple also stores ACL information in forks, but their xattr API
 *    deliberately hides the ACLs, forcing developers to use the acl
 *    API. Linux also stores ACL information in xattrs, but doesn't
 *    hide them from the xattr API, meaning xattr support on Linux is
 *    ACL support.
 *
 */

#ifdef ENABLE_XATTR

#ifdef HAVE_SYS_XATTR_H

#include <sys/xattr.h>

#ifdef XATTR_MAXNAMELEN
#define RADMIND_XATTR_MAXNAMELEN	XATTR_MAXNAMELEN
#elif defined( XATTR_NAME_MAX )
#define RADMIND_XATTR_MAXNAMELEN	XATTR_NAME_MAX
#endif /* XATTR_MAXNAMELEN || XATTR_NAME_MAX */

#elif defined( HAVE_SYS_EXTATTR_H )

#include <sys/extattr.h>
#define RADMIND_XATTR_MAXNAMELEN	EXTATTR_MAXNAMELEN

#else /* !(HAVE_SYS_XATTR_H || HAVE_SYS_EXTATTR_H) */

#define XATTR_MAXNAMELEN	255

#endif /* HAVE_SYS_XATTR_H || HAVE_SYS_EXTATTR_H */

/* baseline buffer size for getxattr */
#define RADMIND_XATTR_BUF_SZ	4096

/* extension for fake xattr paths in transcripts */
#define RADMIND_XATTR_XTN	".xattr"

struct xattrlist {
    char	*x_data;
    ssize_t	x_len;
};

#ifdef __APPLE__

#define xattr_get_size(a,b)	getxattr((a), (b), NULL, 0, 0, XATTR_NOFOLLOW)

#else /* __APPLE__ */

#define xattr_get_size(a,b)	lgetxattr((a), (b), NULL, 0)

#endif /* __APPLE__ */
#endif /* ENABLE_XATTR */

/*
 * xattr_list: return the size of the extended attribute name blob for
 * path. if xlist is non-null, the xattr name list will be copied into
 * it. the result must be freed by the caller.
 */
ssize_t		xattr_list( char *path, char **xlist );

/*
 * xattr_get: get the contents of an xattr. returns the size of the
 * attribute xname or -1 on error. xbuf contains the xattr contents.
 */
ssize_t		xattr_get( char *path, char *xname,
			   char **xbuf, unsigned int offset );

/*
 * xattr_set: set the contents of an xattr. returns 0 on success or
 * -1 on error.
 */
int		xattr_set( char *path, char *name,
			   char *value, size_t size, unsigned int offset );

/*
 * xattr_remove: remove an xattr from a file. returns 0 on succes or
 * -1 on error.
 */
int		xattr_remove( char *path, char *name );

/* return the path of an xattr's parent object or NULL on error. */
char		*xattr_get_path( char * );

/* return the name of an xattr from an xattr path or NULL on error. */
char		*xattr_get_name( char *, char * );

/* return an encoded xattr name. currently only slashes are escaped. */
char		*xattr_name_encode( char * );

/* return a decoded xattr name. currently only escaped slashes are unescaped. */
char		*xattr_name_decode( char * );
