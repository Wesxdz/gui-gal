/**********************************************************************
  test_utils.c
 **********************************************************************

  test_utils - Test GAUL general support functions.
  Copyright ©2003, Stewart Adcock <stewart@linux-domain.com>
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

  Synopsis:	Test GAUL general support functions.

 **********************************************************************/

/*
 * Includes
 */
#include "gaul.h"

/**********************************************************************
  main()
  synopsis:	Test GAUL's general support code.
  parameters:
  return:
  updated:	25 Jul 2003
 **********************************************************************/

int main(int argc, char **argv)
  {

/*
 * Run utility test functions.
 */
  avltree_test();
  table_test();
  linkedlist_test();

  exit(EXIT_SUCCESS);
  }


