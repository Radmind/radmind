SNET * connectsn( char *host, int port );
int closesn( SNET *sn );

int retr( SNET *sn, char *pathdesc, char *path, char *cksumval,
    char *temppath, size_t transize );
int retr_applefile( SNET *sn, char *pathdesc, char *path, 
    char *cksumval, char *temppath, size_t transize );

int n_stor_file( SNET *sn, char *filename, char *transcript );
int stor_file( int fd, SNET *sn, char *filename, char *trancksum,
    char *transcript, char *filetype, size_t transize );
int stor_applefile( int dfd, SNET *sn, char *filename, char *trancksum,
    char *transcript, char *filetype, size_t transize );
