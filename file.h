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
#include <pthread.h>

#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>
#include <cstring>

class File
{
private:
	Dir *directory;
	File *prev, *next; // only per directory, not globally
	size_t fsize; // the file size 

	union {
		uint32_t value; // a random integer will be transformed into
		char checksum[4];    // a checksum

	}id;  // checksum pattern

	pthread_mutex_t mutex;
	char *time_buf; // for ctime_r(time, time_buf)
	string create_time; //create time 

	bool sync_failed; // fsync() or close() failed
	bool has_error;
	bool in_delete; // the write thread is going to delete it, the read thread shall ignore it

	int read_fd(int fd, char *buf, uint64_t &off);

public:
	char fname[9]; // file name
	File(Dir *dir);

	~File(void);

	void fwrite(void);
	void delete_all(void);
	
	void link(File *file);
	void unlink(void);
	int check_fd(int fd);
	int check(void);
	void lock(void);
	void unlock(void);
	int  trylock(void);
	
	File *get_next(void) const;

	int num_checks; // how often this file aready has been verified
	
	size_t get_fsize() const
	{
		return this->fsize;
	}

	bool is_being_deleted(void)
	{
		return this->in_delete;
	}

	void set_in_delete(void)
	{
		this->in_delete = true;
	}

	
};

#endif // __FILE_H__
