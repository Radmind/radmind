
# Process this file with autoconf to produce a configure script.
AC_INIT(radmind, VERSION, radmind@umich.edu)
AC_PREREQ(2.52)
AC_COPYRIGHT([Copyright (c) 1998 Regents of The University of Michigan.  All Rights Reserved.])
AC_CONFIG_SRCDIR([applefile.c])
#AC_CONFIG_HEADER([config.h])

# Set up variables
AC_ARG_VAR(diffpath, [path to diff utility])
AC_ARG_WITH(server, AC_HELP_STRING([--with-server=server], [default radmind server]), server="$withval", server="radmind" )
AC_SUBST(server) 

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PATH_PROG(diffpath, diff)

# Checks for libraries.
AC_CHECK_LIB([nsl], [inet_ntoa])
AC_CHECK_LIB([socket], [socket])

CHECK_SSL
CHECK_SNET

# Checks for header files.
#AC_HEADER_DIRENT
#AC_HEADER_STDC
#AC_HEADER_MAJOR
#AC_HEADER_SYS_WAIT
#AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h syslog.h unistd.h utime.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
#AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_LSTAT
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([dup2 gethostbyaddr gethostbyname gethostname gettimeofday inet_ntoa memset mkdir rmdir select socket strcasecmp strchr strdup strerror strncasecmp strrchr strstr strtol utime herror])

AC_CONFIG_FILES([Makefile
                 libsnet/Makefile])
AC_OUTPUT
