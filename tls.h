int tls_client_setup( int use_randfile, int authlevel, char *ca, char *cert, char *privatekey );
int tls_client_start( SNET *sn, char *host, int authlevel );
