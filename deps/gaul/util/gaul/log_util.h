/**********************************************************************
  log_util.h
 **********************************************************************

  log_util - MT safe, MP aware, general logging functions.
  Copyright ©2000-2006, Stewart Adcock <stewart@linux-domain.com>
  All rights reserved.

  The latest version of this program should be available at:
  http://www.stewart-adcock.co.uk/

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.  Alternatively, if your project
  is incompatible with the GPL, I will probably agree to requests
  for permission to use the terms of any other license.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY WHATSOEVER.

  A full copy of the GNU General Public License should be in the file
  "COPYING" provided with this distribution; if not, see:
  http://www.gnu.org/

 **********************************************************************

  Synopsis:	Header file for my general logging functions.

 **********************************************************************/

#ifndef LOG_UTIL_H_INCLUDED
#define LOG_UTIL_H_INCLUDED

#include "gaul/gaul_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "gaul/memory_util.h"

#ifdef HAVE_MPI
#include <mpi.h>
#endif

/*
 * Constants
 */
#define	LOG_MAX_LEN	160

/*
 * Log message type tags.
 */
enum log_level_type {
  LOG_NONE=0,	/* No messages - I recommend that this isn't used! */
  LOG_FATAL,	/* Fatal errors only */
  LOG_WARNING,	/* Warning messages */
  LOG_QUIET=3,	/* Normal messages */
  LOG_NORMAL=3,	/*  - " - */
  LOG_VERBOSE,	/* Verbose messages */
  LOG_FIXME,	/* Development reminders */
  LOG_DEBUG	/* Debugging messages */
  };

/*
 * Function prototypes
 */
/* Function definition for custom output function: */
GAULFUNC typedef void	(*log_func)(const enum log_level_type level,
                          const char *func_name,
                          const char *file_name,
                          const int line_num,
                          const char *message);

GAULFUNC void	log_init(enum log_level_type level, char *fname, log_func func, boolean date);
GAULFUNC void	log_set_level(enum log_level_type level);
GAULFUNC void	log_set_file(const char *fname);
GAULFUNC enum log_level_type	log_get_level(void);

/*
 * This is the actual logging function, but isn't intended to be used
 * directly.
 */
GAULFUNC void	log_output( const enum log_level_type level,
                          const char *func_name,
                          const char *file_name,
                          const int line_num,
                          const char *format, ... );

/*
 * Main plog macro.  Should replace this with an inline
 * function?  Especially since log_get_level() is an
 * inline function anyway.
 *
 * Unfortunately, this nice macro will only work on GNU systems.
 * We have a less-functional replacement for other systems.
 */
#if ( defined(__GNUC__) || defined(__INTEL_COMPILER) ) && !defined(__APPLE_CPP__) && !defined(__APPLE_CC__)
#define	plog( level, ... ) { 	\
	if ( (level) <= log_get_level() ) 	\
	  log_output(level, __PRETTY_FUNCTION__,	\
	                   __FILE__, __LINE__,		\
	                   ##__VA_ARGS__); }
#else
GAULFUNC void plog( const enum log_level_type level, const char *format, ... );
#endif

/*
 * SLang intrinsic function with equivalent functionality.
 */
#ifdef HAVE_SLANG
GAULFUNC void	log_wrapper(int *level, char *message);
#endif

#endif /* LOG_UTIL_H_INCLUDED */


