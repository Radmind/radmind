#define UPDATEUNIT	1024

off_t loadsetsize( FILE *tran );
off_t applyloadsetsize( FILE *tran );
void progressupdate( ssize_t bytes, char *path );
