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

#include "fstest.h"
#include "dir.h"

using namespace std;

Dir::Dir(Dir *parent, int num) : parent(parent)
{
	fs = parent->fs;
	next = parent->sub;
	sub = NULL;
	files = NULL;
	num_files = 0;

	dirname = "d000";
	dirname[1] = '0' + num / 10;
	dirname[2] = '0' + num % 10;

	parent->sub = this;
	string dirpath = path();
	cout << "Creating dir " << dirpath << endl;
	if (mkdir(dirpath.c_str(), 0700) != 0) {
		cout << "Creating dir " << path();
		perror(": ");
		EXIT(1);
	}

	fs->all_dirs.push_back(this);
	fs->active_dirs.push_back(this);

	while(--num > 0) {
		new Dir(this, num);
	}
}

Dir::Dir(string _path, Filesystem * fs): fs(fs)
{
	parent = NULL;
	next = NULL;
	sub = NULL;
	files = NULL;
	num_files = 1;
	root_path = _path;

	if (mkdir(path().c_str(), 0700) != 0) {
		cout << "Creating working dir " << path();
		perror(": ");
		EXIT(1);
	}
	fs->all_dirs.push_back(this);
	fs->active_dirs.push_back(this);
}

Dir::~Dir(void)
{
	cout << "~Dir(" << path() << ")\n";
	if (next != NULL) delete next;
	if (sub != NULL) delete sub;
	if (files != NULL) {
		files->delete_all();
	}
	int res = rmdir(path().c_str());
	if (res != 0 && errno != ENOENT) {
		perror(path().c_str());
		EXIT(1);
	}
}

void Dir::add_file(File *file)
{
	file->link(files);
	this->files = file;
	this->num_files++;
}

void Dir::remove_file(File *file)
{
	this->num_files--;
	if (this->files == file) 
		files = files->get_next();
	file->unlink();
}

string Dir::path(void) const
{
	if (parent == NULL) {
		return root_path;
	} else {
		// recursion!
		return parent->path() + dirname + "/";
	}
}

uint16_t Dir::get_num_files(void) const
{
	return num_files;
}

