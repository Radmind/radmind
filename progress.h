#define PROGRESSUNIT	1024

void linecheck( char *line, int ac, int linenum );
off_t loadsetsize( FILE *tran );
off_t applyloadsetsize( FILE *tran );
off_t lcksum_loadsetsize( FILE *tran, char *prefix );
void progressupdate( ssize_t bytes, char *path );
