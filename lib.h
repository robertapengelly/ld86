/******************************************************************************
 * @file            lib.h
 *****************************************************************************/
#ifndef     _LIB_H
#define     _LIB_H

#include    <stddef.h>

char *xstrdup (const char *str);

int strstart (const char *val, const char **str);
int xstrcasecmp (const char *s1, const char *s2);

void *xmalloc (unsigned long size);
void *xrealloc (void *ptr, unsigned long size);

void dynarray_add (void *ptab, unsigned long *nb_ptr, void *data);
void parse_args (int *pargc, char ***pargv, int optind);

#endif      /* _LIB_H */
