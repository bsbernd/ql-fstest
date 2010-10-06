/************************************************************************
 *
 * Filesystem stress and verify
 *
 * Authors: Goswin von Brederlow <brederlo@informatik.uni-tuebingen.de>
 *          Bernd Schubert <bschubert@ddn.com>
 *
 * Copyright (C) 2007 Q-leap Networks, Goswin von Brederlow
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *    USA
 *
 ************************************************************************/

#ifndef __FILE_H__
#define __FILE_H__

#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>
#include <cstring>

class File
{
private:
	Dir *directory;
	File *prev, *next;
	size_t fsize; // the file size 
	uint32_t id;
	char fname[9]; // file name
public:
	File(Dir *dir, uint64_t num);

	~File(void);
	
	void delete_all(void);
	
	void link(File *file);
	void unlink(void);
	void check(void);
	
	File *get_next(void) const;
	size_t get_fsize(void) const;

};

#endif // __FILE_H__
