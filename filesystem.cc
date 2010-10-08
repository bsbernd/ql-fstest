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

#include "fstest.h"

static int size_min = 20; // 1MiB
static int size_max = 22; // 1GiB
static int stats_interval = 60;
//int size_max = 35; // 32GiB
//int stats_interval = 900;

using namespace std;

/* filesystem constructor */
Filesystem::Filesystem(string dir, double percent)
{
	this->goal_percent = percent;
	pthread_mutex_init(&this->mutex, NULL);
	this->error_detected = false;
	
	// Create working dir
	root_dir = new Dir(dir, this);
	this->update_stats();

	fs_use_goal = this->fssize * percent;
	
	cout << "Filesystem size     : " << fssize << endl;
	cout << "Filesystem free     : " << fsfree << endl;
	cout << "Filesystem used     : " << 100 * double(fssize - fsfree) / fssize << endl;
	if (double(fssize - fsfree) / fssize >= goal_percent) {
		cerr 	<< "Error: Filesystem already above % used goal: " 
			<< double(fssize - fsfree) / fssize << " >= " 
			<< goal_percent << endl;
		EXIT(1);
	}
	
	
	was_full = false;
}

Filesystem::~Filesystem(void)
{
	this->lock();
	if (this->root_dir != NULL && !this->error_detected) {
		delete root_dir;
		root_dir = NULL;
	}
	this->unlock();
	pthread_mutex_destroy(&this->mutex);
}

/* Update statistics about this filesystem 
 * Filesystem has to be locked */
void Filesystem::update_stats(void)
{
	struct statvfs statvfsbuf;

	// Get FS stats
	if (statvfs(root_dir->path().c_str(), &statvfsbuf) != 0) {
		perror("statvfs(): ");
		EXIT(1);
	}

	fssize = statvfsbuf.f_blocks * statvfsbuf.f_frsize;
	fsfree = statvfsbuf.f_bavail * statvfsbuf.f_frsize;
	
	memset(&stats_old, 0, sizeof(stats_old));
	memset(&stats_now, 0, sizeof(stats_now));
	
	stats_old.time = time(NULL);
	stats_now.time = time(NULL);
}


/**
 * free some disk space if usage above goal
 */
void Filesystem::free_space(size_t fsize)
{
	if (this->error_detected)
		pthread_exit(NULL); // Don't delete anything, just exit now
	
	struct statvfs vfsbuf;
		
	if (statvfs(root_dir->path().c_str(), &vfsbuf) != 0) {
		perror("statvfs()");
		EXIT(1);
	}
	uint64_t fsfree = vfsbuf.f_bavail * vfsbuf.f_frsize;

	while(this->fssize - fsfree - fsize > this->fs_use_goal)
	{
		this->was_full = true;
		// Remove a file
	
		int retry_count = 0;
retry:
		this->lock();
		int nfiles = files.size();
		if (nfiles < 5) {
			this->unlock();
			return; // no files left to delete
		}
		int num = random() % nfiles;
		
		File *file = files[num];

		// Don't delete a file that is in read or not checked yet
		// Our read loop does not like that
		int rc = file->trylock();
		if (rc && rc == EBUSY) {
			// File probably just in read
			this->unlock();
			sleep(1);
			retry_count++;
			
			if (retry_count < 10 && nfiles > 10)
				goto retry;
			else
				return;
		}
		this->unlock();
		
		string fname = file->fname;
		int nchecks = file->num_checks;
				
		if (nchecks == 0) {
			file->unlock();
			if (this->files.size() > 2) {
#ifdef DEBUG
				cerr << fname
					<< " num_checks: "
					<< nchecks << endl;
#endif
				sleep(1);
				goto retry; // try another file
			} else {
				// We need more files before we can delete one
				return;
			}
		}
		
		if (nchecks < 3) {
			// check the file a last time
			if (file->check()) {
				this->error_detected = true;
				// we exit the write thread
				pthread_exit(NULL);
			}
		}
		
		this->lock();
		delete file; // will also unlock the file
		files[num] = files[files.size() - 1];
		files.resize(files.size() - 1);
		this->unlock();
	}
}

/** write thread
 * when it deletes a file, it will start a read, though
 */
void Filesystem::write(void)
{
	int level = 1;
	int max_files = 1;
	new Dir(root_dir, 1);
	cout << "Starting test       : " << ctime(&stats_old.time);

	while(this->error_detected == false) {
		// cout << "all_dirs: " << all_dirs.size() << endl;
		// cout << "active_dirs: " << active_dirs.size() << endl;

		// Pick a random file size
		size_t fsize = 1ULL << (size_min + random() % (size_max - size_min + 1));
		// cout << "Size: " << size << endl;

		// free some space by deleting a file,
		this->free_space(fsize);

		// Pick a random directory
		int num = random() % active_dirs.size();
		// cout << "Picked " << active_dirs[num]->path() << endl;

		this->lock();
		// Create file
		File *file = new File(active_dirs[num], fsize);
		file->lock();
		this->unlock();
		file->fwrite();
		file->unlock();
		
		this->lock();
		stats_now.write += fsize;
		stats_now.num_files++;
		// cout << "dir->num_files: " << active_dirs[num]->fsize() << endl;
		// cout << "files: " << files.size() << endl;

		// Remove dir from active_dirs if full
		if (active_dirs[num]->get_num_files() >= max_files) {
			active_dirs[num] = active_dirs[active_dirs.size() - 1];
			active_dirs.resize(active_dirs.size() - 1);
		}

		if (active_dirs.size() == 0) {
			++level;
			max_files = level * level;
			cout << "Going to level " << level << endl;
			active_dirs = all_dirs;
			new Dir(root_dir, level);
		};

		// Output stats
		stats_now.time = time(NULL);
		if (stats_now.time - stats_old.time > stats_interval) {
			double t = stats_now.time - stats_old.time;
			double write = (stats_now.write - stats_old.write) / t / MEGA;
			double read = (stats_now.read - stats_old.read) / t / MEGA;
			double files = (stats_now.num_files - stats_old.num_files) / t;
			cout << stats_now.time << " write: " << stats_now.write / GIGA 
				<< " GiB [" << write << " MiB/s] read: " << stats_now.read / GIGA 
				<< " GiB [" << read << " MiB/s] Files: " << stats_now.num_files 
				<< " [" << files << " files/s] # " << ctime(&stats_now.time);
			
			cout.flush();
			this->update_stats();
		}
		this->unlock();
	}
}

/* Read from the beginning to the end, if end is reached
 * continue at the beginning
 */
void Filesystem::read_loop(void)
{
	unsigned long index = 0;
start_again:
	this->lock();
	// wait until the 2nd file is being written
	while (this->files.size() < 2) {
		this->unlock();
		pthread_yield();
		sleep (1);
		goto start_again;
	}
	
#ifdef DEBUG
	cerr << "Starting to read files" << endl;
#endif
	
	if (this->files.size() < 2) {
		// Arg, already deleted again!
		this->unlock();
	}
	
	File *file = this->files.at(index);
	// now lock the file, still a chance of a race, until the unlink()
	// methods also lock the filesystem first -> FIXME
	file->lock(); 
	this->unlock();

	while(true) {
		// file is locked here
		
		if (file->check())
			this->error_detected = true;
		int fsize = file->get_fsize();
		file->unlock();

		this->lock();
		this->stats_now.read += fsize;
		this->unlock();
		index++;
newindex:
		this->lock();
		unsigned long current_num_files = this->files.size();
		if (index >= current_num_files) {
			this->unlock();
			// last file, restart from beginning
			if (!this->was_full)
				sleep(20); // give it some time to write new data
			cout << "Re-starting to read from index 0 (was "
				<< index << ")" << endl;
			index = 0;
			goto newindex;
		}
		
		File *tmp;
		tmp = this->files.at(index);
		if (tmp == file) {
			this->unlock();
			continue; // re-read the current file
		}
		
		if (tmp->trylock()) {
			this->unlock();
			// file is busy, take next file
			index++;
			goto newindex;
		}

		this->unlock();
		file = tmp;
	}
	
	
}


void Filesystem::lock(void)
{
	int rc = pthread_mutex_lock(&this->mutex);
	if (rc) {
		cerr << "Failed to lock filesystem" << " : " << strerror(rc);
		perror(": ");
		EXIT(1);
	}
}

void Filesystem::unlock(void)
{
	int rc = pthread_mutex_unlock(&this->mutex);
	if (rc) {
		cerr << "Failed to lock filesystem" << " : " << strerror(rc);
		perror(": ");
		EXIT(1);
	}
}

int Filesystem::trylock(void)
{
	int rc = pthread_mutex_trylock(&this->mutex);
	if (rc && rc != EBUSY) {
		cerr << "Failed to lock filesystem" << " : " << strerror(rc);
		perror(": ");
		EXIT(1);
	}
	return rc;
}


