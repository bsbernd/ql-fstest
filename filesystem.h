/************************************************************************
 *
 * Filesystem stress and verify
 *
 * Authors: Goswin von Brederlow <brederlo@informatik.uni-tuebingen.de>
 *          Bernd Schubert <bschubert@ddn.com>
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

#include <pthread.h>

#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

struct StatsStamp {
	time_t time;
	uint64_t write, read;
	uint64_t num_files, num_read_files, num_written_files;
};

class Filesystem 
{
private:
	Dir *root_dir;

	uint64_t fssize;
	uint64_t fsfree;
	uint64_t fs_use_goal;
	double goal_percent;
	bool was_full;
	unsigned long last_read_index; // last index read in
	
	StatsStamp stats_old;
	StatsStamp stats_now;
	StatsStamp stats_all;
	
	void update_stats(void);
	void free_space(size_t fsize);
	
	volatile bool error_detected;
		
	// protect file and directory addition/removal and stats
	pthread_mutex_t mutex;
public:
	Filesystem(string dir, double percent);
	~Filesystem(void);
	
	void write(void);
	void read_loop(void);
	
	// Global options
	vector<Dir*> all_dirs;
	vector<Dir*> active_dirs;
	vector<File*> files;

	void lock(void);
	void unlock(void);
	int  trylock(void);
};

#endif // __FILESYSTEM_H__
