/**********************************************************************
  ga_chromo.h
 **********************************************************************

  ga_chromo - Genetic algorithm chromosome handling routines.
  Copyright ©2000-2006, Stewart Adcock <stewart@linux-domain.com>
  All rights reserved.

  The latest version of this program should be available at:
  http://gaul.sourceforge.net/

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

  Synopsis:     Header file for the routines for handling GAUL's
		built-in chromosome types.

 **********************************************************************/

#ifndef GA_CHROMO_H_INCLUDED
#define GA_CHROMO_H_INCLUDED

#include "gaul.h"

GAULFUNC boolean ga_chromosome_integer_allocate(population *pop, entity *embryo);
GAULFUNC void ga_chromosome_integer_deallocate(population *pop, entity *corpse);
GAULFUNC void ga_chromosome_integer_replicate( const population *pop,
                                      entity *parent, entity *child,
                                      const int chromosomeid );
GAULFUNC unsigned int ga_chromosome_integer_to_bytes(const population *pop, entity *joe,
                                     gaulbyte **bytes, unsigned int *max_bytes);
GAULFUNC void ga_chromosome_integer_from_bytes(const population *pop, entity *joe, gaulbyte *bytes);
GAULFUNC char *ga_chromosome_integer_to_string(const population *pop, const entity *joe, char *text, size_t *textlen);

GAULFUNC boolean ga_chromosome_boolean_allocate(population *pop, entity *embryo);
GAULFUNC void ga_chromosome_boolean_deallocate(population *pop, entity *corpse);
GAULFUNC void ga_chromosome_boolean_replicate( const population *pop,
                                      entity *parent, entity *child,
                                      const int chromosomeid );
GAULFUNC unsigned int ga_chromosome_boolean_to_bytes(const population *pop, entity *joe,
                                    gaulbyte **bytes, unsigned int *max_bytes);
GAULFUNC void ga_chromosome_boolean_from_bytes(const population *pop, entity *joe, gaulbyte *bytes);
GAULFUNC char *ga_chromosome_boolean_to_string(const population *pop, const entity *joe, char *text, size_t *textlen);

GAULFUNC boolean ga_chromosome_double_allocate(population *pop, entity *embryo);
GAULFUNC void ga_chromosome_double_deallocate(population *pop, entity *corpse);
GAULFUNC void ga_chromosome_double_replicate( const population *pop,
                                      entity *parent, entity *child,
                                      const int chromosomeid );
GAULFUNC unsigned int ga_chromosome_double_to_bytes(const population *pop, entity *joe,
                                    gaulbyte **bytes, unsigned int *max_bytes);
GAULFUNC void ga_chromosome_double_from_bytes(const population *pop, entity *joe, gaulbyte *bytes);
GAULFUNC char *ga_chromosome_double_to_string(const population *pop, const entity *joe, char *text, size_t *textlen);

GAULFUNC boolean ga_chromosome_char_allocate(population *pop, entity *embryo);
GAULFUNC void ga_chromosome_char_deallocate(population *pop, entity *corpse);
GAULFUNC void ga_chromosome_char_replicate( const population *pop,
                                      entity *parent, entity *child,
                                      const int chromosomeid );
GAULFUNC unsigned int ga_chromosome_char_to_bytes(const population *pop, entity *joe,
                                    gaulbyte **bytes, unsigned int *max_bytes);
GAULFUNC void ga_chromosome_char_from_bytes(const population *pop, entity *joe, gaulbyte *bytes);
GAULFUNC char *ga_chromosome_char_to_string(const population *pop, const entity *joe, char *text, size_t *textlen);

GAULFUNC boolean ga_chromosome_bitstring_allocate(population *pop, entity *embryo);
GAULFUNC void ga_chromosome_bitstring_deallocate(population *pop, entity *corpse);
GAULFUNC void ga_chromosome_bitstring_replicate( const population *pop, entity *parent, entity *child, const int chromosomeid );
GAULFUNC unsigned int ga_chromosome_bitstring_to_bytes(const population *pop, entity *joe, gaulbyte **bytes, unsigned int *max_bytes);
GAULFUNC void ga_chromosome_bitstring_from_bytes(const population *pop, entity *joe, gaulbyte *bytes);
GAULFUNC char *ga_chromosome_bitstring_to_string(const population *pop, const entity *joe, char *text, size_t *textlen);

GAULFUNC boolean ga_chromosome_list_allocate(population *pop, entity *embryo);
GAULFUNC void ga_chromosome_list_deallocate(population *pop, entity *corpse);
GAULFUNC void ga_chromosome_list_replicate( const population *pop, entity *parent, entity *child, const int chromosomeid );
GAULFUNC unsigned int ga_chromosome_list_to_bytes(const population *pop, entity *joe, gaulbyte **bytes, unsigned int *max_bytes);
GAULFUNC void ga_chromosome_list_from_bytes( const population *pop, entity *joe, gaulbyte *bytes );
GAULFUNC char *ga_chromosome_list_to_string( const population *pop, const entity *joe, char *text, size_t *textlen);

#endif /* GA_CHROMO_H_INCLUDED */

