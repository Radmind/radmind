/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */
struct syslogname {
    char        *sl_name;
    int         sl_value;
};

extern struct syslogname        _syslogfacility[], _sysloglevel[];
int                             syslogname( char *, struct syslogname * );

#define syslogfacility(x)       syslogname((x),_syslogfacility)
#define sysloglevel(x)          syslogname((x),_sysloglevel)
