/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifndef SIZEOF_OFF_T
#error "sizeof off_t unknown."
#endif

#if SIZEOF_OFF_T == 8
    #ifdef HAVE_STRTOLL
    #define strtoofft(x,y,z)	(strtoll((x),(y),(z)))
    #else
    #define strtoofft(x,y,z)        (strtol((x),(y),(z)))
    #endif
#define PRIofft			"ll"
#else	/* a bit of an assumption, here */
#define strtoofft(x,y,z)	(strtol((x),(y),(z)))
#define PRIofft			"l"
#endif
