// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   SaveEntry - Contains the save entry object for temporary state

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_SAVEENTRY_H
#define PKGLIB_SAVEENTRY_H

#include <apt-pkg/macros.h>

struct APT_PUBLIC SaveEntry
{
   char prefix;
   char *package;
};

#endif
