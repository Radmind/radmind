#define PROGRESSUNIT	1024

off_t loadsetsize( FILE *tran );
off_t applyloadsetsize( FILE *tran );
off_t lcksum_loadsetsize( FILE *tran, char *prefix );
void progressupdate( ssize_t bytes, char *path );
