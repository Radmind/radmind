#define argcargv(X, Y) (acav_parse( NULL, X, Y ))

typedef struct {
    unsigned acv_argc;
    char **acv_argv;
} ACAV;
ACAV* acav_alloc( void );
int acav_parse( ACAV *acav, char *, char *** );
int acav_free( ACAV *acav );
