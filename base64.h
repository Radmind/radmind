/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#define SZ_BASE64_E( x )	(((x)+2)/3*4+1)
#define SZ_BASE64_D( x )	(((x)*3)/4)

void	base64_e( unsigned char *, int, char * );
void	base64_d( char *, int, unsigned char * );
