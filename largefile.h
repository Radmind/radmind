/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#if SIZE_OFF_T == 8
#define strtoofft(x,y,z)	(strtoll((x),(y),(z)))
#define PRIofft			"ll"
#else	/* a bit of an assumption, here */
#define strtoofft(x,y,z)	(strtol((x),(y),(z)))
#define PRIofft			"l"
#endif
