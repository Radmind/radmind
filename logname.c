/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <syslog.h>
#include <string.h>

#include "logname.h"

struct syslogname	_sysloglevel[] = {
    { "emerg",          LOG_EMERG },
    { "alert",          LOG_ALERT },
    { "crit",           LOG_CRIT },
    { "err",            LOG_ERR },
    { "warning",        LOG_WARNING },
    { "notice",         LOG_NOTICE },
    { "info",           LOG_INFO },
    { "debug",          LOG_DEBUG },
    { 0,                0 },
};

struct syslogname 	_syslogfacility[] = {
#ifdef LOG_KERN
    { "kern",		LOG_KERN },
#endif // LOG_KERN
    { "user",		LOG_USER },
    { "mail",		LOG_MAIL },
    { "daemon",		LOG_DAEMON },
    { "auth",		LOG_AUTH },
    { "syslog",		LOG_SYSLOG },
    { "lpr",		LOG_LPR },
    { "news",		LOG_NEWS },
    { "uucp",		LOG_UUCP },
    { "cron",		LOG_CRON },
#ifdef LOG_FTP
    { "ftp",		LOG_FTP },
#endif // LOG_FTP
#ifdef LOG_AUTHPRIV
    { "authpriv",	LOG_AUTHPRIV },
#endif // LOG_AUTHPRIV
    { "local0",		LOG_LOCAL0 },
    { "local1",		LOG_LOCAL1 },
    { "local2",		LOG_LOCAL2 },
    { "local3",		LOG_LOCAL3 },
    { "local4",		LOG_LOCAL4 },
    { "local5",		LOG_LOCAL5 },
    { "local6",		LOG_LOCAL6 },
    { "local7",		LOG_LOCAL7 },
    { 0,		0 },
};

    int
syslogname( char *name, struct syslogname *sln )
{
    for ( ; sln->sl_name != 0; sln++ ) {
	if ( strcasecmp( sln->sl_name, name ) == 0 ) {
	    return( sln->sl_value );
	}
    }
    return( -1 );
}
