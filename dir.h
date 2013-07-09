/************************************************************************
 *
 * Filesystem stress and verify
 *
 * Authors: Goswin von Brederlow <brederlo@informatik.uni-tuebingen.de>
*          Bernd Schubert <bernd.schubert@fastmail.fm>
  *
 * Copyright (C) 2007 Q-leap Networks, Goswin von Brederlow
 *               2010 DataDirect Networks, Bernd Schubert
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

#ifndef __DIR_H__
#define __DIR_H__

#include "fstest.h"

class Filesystem;
class File;

class Dir
{
private:
	Dir *parent;
	Dir *next;
	Dir *sub;
	File *files;
	string dirname;
	uint16_t num_files;
	string root_path;
public:
	Filesystem *fs;
	Dir(Dir *parent, int num);
	Dir(string _path, Filesystem *fs) ;
	
	~Dir(void);
	
	string path(void) const;

	void add_file(File *file);
	void remove_file(File *file);

	uint16_t get_num_files(void) const;
};

#endif // __DIR_H__ 
