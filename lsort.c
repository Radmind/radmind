#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int exitrc;

/************* start stuff from radmind source code ************************/
/* Just like strcmp(), but pays attention to the meaning of '/'.  */
    int 
pathcmp( const unsigned char *p1, const unsigned char *p2 )
{
    int		rc;

    do {
	if (( rc = ( *p1 - *p2 )) != 0 ) {
	    if (( *p2 != '\0' ) && ( *p1 == '/' )) {
		return( -1 );
	    } else if (( *p1 != '\0' ) && ( *p2 == '/' )) {
		return( 1 );
	    } else {
		return( rc );
	    }
	}
	p2++;
    } while ( *p1++ != '\0' );

    return( 0 );
}

    char *
decode( char *line ) 
{
    /* static */
    static char     buf[ MAXPATHLEN ];
    char	    *temp;

    if ( strlen( line ) > ( 2 * MAXPATHLEN )) {
	return( NULL );
    }

    temp = buf;

    for ( ; *line != '\0'; line++, temp++ ) {
	switch( *line ) {
	case '\\':
	    line++;
	    switch( *line ) {
	    case 'n':
		*temp = '\n';
		break;
	    case 't':
		*temp = '\t';
		break;
	    case 'b':
		*temp = ' ';
		break;
	    case 'r':
		*temp = '\r';
		break;
	    case '\\':
		*temp = '\\';
		break;
	    default:
		break;
	    }
	    break;
	default:
	    *temp = *line;
	    break;
	}
    }

    *temp = '\0';
    return( buf );
}

/************* end stuff from radmind source code ************************/

struct save_line {
	struct save_line *next;
	char *key;
	char *data;
} *lines;
int linecount;

    void
save_it( char *buffer, char *pathname)
{
	struct save_line *sp;
	sp = malloc(sizeof *sp + strlen(buffer) + strlen(pathname) + 4);
	sp->key = (char*) (sp+1);
	strcpy(sp->key, pathname);
	sp->data = (sp->key + strlen(sp->key) + 1);
	strcpy(sp->data, buffer);
	sp->next = lines;
	lines = sp;
	++linecount;
}

    int
lsort_cmp( void *a1, void *b1 )
{
	const struct save_line **a, **b;

	a = a1;
	b = b1;
	return pathcmp((*a)->key, (*b)->key);
}

    void
sort_them()
{
	struct save_line **x, *sp, **y;
	x = (struct save_line**) malloc(sizeof *x * linecount);
	y = x;
	for (sp = lines; sp; sp = sp->next)
		*y++ = sp;
	qsort(x, linecount, sizeof *x, lsort_cmp);
	sp = 0;
	while (y-- != x)
	{
		(*y)->next = sp;
		sp = (*y);
	}
	lines = sp;
}

    void
print_them()
{
	struct save_line *sp;
	for (sp = lines; sp; sp = sp->next)
	{
		fputs(sp->data, stdout);
		if (ferror(stdout))
		{
			perror("writing stdout");
			exitrc |= 4;
			break;
		}
	}
}

    void
process( char * arg )
{
	FILE *fd;
	char buffer[4096];
	char control[4096];
	char pathname[4096];
	char *fn;
	char *cp;
	int lineno;

	if (strcmp(arg, "-"))
	{
		fn = arg;
		fd = fopen(arg, "r");
	}
	else
	{
		fn = "(stdin)";
		fd = stdin;
	}
	if (!fd)
	{
		perror(arg);
		exitrc |= 1;
		return;
	}

	lineno = 0;
	while (fgets(buffer, sizeof buffer, fd))
	{
		++lineno;
		cp = buffer;
		if (*cp == '-' && cp[1] == ' ') cp += 2;
		if (sscanf(cp, "%s %s ", control, pathname) != 2)
		{
fprintf(stderr,"%s(%d): discarding because not enough fields\n", fn, lineno);
			exitrc |= 2;
			continue;
		}
		save_it(buffer,decode(pathname));
	}

	if (fd == stdin)
		clearerr(fd);
	else
		fclose(fd);
}

    int
main( int argc, char **argv)
{
	char *argp;
	int didit;

	didit = 0;

	while (--argc > 0) if (*(argp = *++argv) == '-')
	if (argp[1]) while (*++argp) switch(*argp)
	{
	case '-':
		break;
	default:
		fprintf (stderr,
			"Usage: lsort [files...]\n");
		exit(1);
	}
	else ++didit, process("-");
	else ++didit, process(argp);
	if (!didit) process("-");
	sort_them();
	print_them();
	exit(exitrc);
}
