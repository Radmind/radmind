/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

int tls_server_setup( int use_randfile, int authlevel, char *caFile, char *caDir, char *cert, char *privatekey );
int tls_client_setup( int use_randfile, int authlevel, char *caFile, char *caDir, char *cert, char *privatekey );
int tls_client_start( SNET *sn, char *host, int authlevel );
