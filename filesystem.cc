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

#include "fstest.h"

static int size_min = 20; // 1MiB
static int size_max = 30; // 1GiB
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
	memset(&stats_all, 0, sizeof(stats_all));
	this->update_stats();

	this->fs_use_goal = (double) this->fssize * percent;
	
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

#ifdef DEBUG
	cout 	<< "statvfsbuf.f_blocks=" << statvfsbuf.f_blocks <<endl
	     	<< "statvfsbuf.f_frsize=" << statvfsbuf.f_frsize <<endl
		<< "statvfsbuf.f_bavail=" << statvfsbuf.f_bavail 
		<< endl;
#endif
	fssize = (uint64_t) statvfsbuf.f_blocks * statvfsbuf.f_frsize;
	fsfree = (uint64_t) statvfsbuf.f_bavail * statvfsbuf.f_frsize;
	
	// TODO:  overload an op?*
	this->stats_all.read += this->stats_now.read;
	this->stats_all.write += this->stats_now.write;
	this->stats_all.num_files += this->stats_now.num_files;
	this->stats_all.num_read_files += this->stats_now.num_read_files;
	this->stats_all.num_written_files += this->stats_now.num_written_files;
	
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
		pthread_exit(NULL); // Don't delete anything, just exit immediately
	
	struct statvfs vfsbuf;
		
	if (statvfs(root_dir->path().c_str(), &vfsbuf) != 0) {
		perror("statvfs()");
		EXIT(1);
	}
	double fsfree = vfsbuf.f_bavail * vfsbuf.f_frsize;

	int retry_count = 0;
	while((double) this->fssize - fsfree - (double) fsize > (double)this->fs_use_goal 
		&& retry_count < 20 
		&& this->files.size() > 2)
	{
		retry_count++;
		
		if (!this->was_full) {
			this->was_full = true;
			cout << "Going into write/delete mode" << endl;
		}
		// Remove a file
			
		this->lock();
		int nfiles = files.size();
		if (nfiles < 1) {
			this->unlock();
			return; // no files left to delete
		}
		int num = random() % nfiles;
		
		File *file = files[num];

		// Don't delete a file that is in read or not checked yet
		// Our read loop does not like that
		if (file->trylock()) {
			// File probably just in read
			this->unlock();
			sleep(1);
			continue;
		}
		this->unlock();
		
		string fname = file->fname;
		int nchecks = file->num_checks;
			
		// Check if read-thread already has verified it
		if (nchecks == 0) {
			file->unlock();
			sleep(1);
			continue;
		}
		
		// check the file a last time
		if (nchecks < 10 && file->check()) {
			this->error_detected = true;
			// we exit the write thread
			pthread_exit(NULL);
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
		this->stats_now.write += fsize;
		this->stats_now.num_files++;
		this->stats_now.num_written_files++;
		// cout << "dir->num_files: " << active_dirs[num]->fsize() << endl;
		// cout << "files: " << files.size() << endl;

		// Remove dir from active_dirs if full
		if (active_dirs[num]->get_num_files() >= max_files) {
			this->active_dirs[num] = this->active_dirs[active_dirs.size() - 1];
			this->active_dirs.resize(this->active_dirs.size() - 1);
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
				<< " [" << files << " files/s] # " << ctime(&stats_now.time)
				<< " idx write: " << this->files.size()
				<< " idx read: " << this->last_read_index 
				<< endl;
			
			cout.flush();
			this->update_stats();
		}
		this->unlock();
		
		// Some filesystems prefer writes over reads. But we don't want
		// to let the reads to fall too far behind. Use arbitrary limit
		// of 20 files
		if (!this->was_full) {
			while (this->last_read_index + 100 < this->files.size())
				sleep(1);
		} else {
			while  (this->stats_now.num_written_files > this->stats_now.num_read_files + 20)
				sleep(1);
		}
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
		this->last_read_index = index;
		this->stats_now.read += fsize;
		this->stats_now.num_read_files++;
		this->unlock();

		// no lock here anymore
newindex:
		index++;

		if (this->was_full == false) {
			while (index + 20 >= this->files.size()) {
				// we want writes to be slightly ahead of reads
				// due to the page cache
				sleep(1); // give it some time to write new data
				if (this->was_full) {
					// Filesystem was full
					cout << "Re-starting to read from index 0 (was "
						<< index << ")" << endl;
					index = 0;
					break;
				}
					
			}
		} else if (index >= this->files.size()) {
			// Filesystem was full
			cout << "Re-starting to read from index 0 (was "
				<< index << ")" << endl;
			index = 0;
		}
		
		this->lock();
		
		// double check, now that we have locked it
		if (index >= this->files.size()) {
			// the write thread deleted our file...
			this->unlock();
			index--;
			goto newindex;
		}
		
		File *tmp;
		tmp = this->files.at(index);
		
		if (tmp->trylock()) {
			this->unlock();
			// file is busy, take next file
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


