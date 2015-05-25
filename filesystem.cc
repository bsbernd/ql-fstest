/************************************************************************
 *
 * Filesystem stress and verify
 *
 * Authors: Goswin von Brederlow <brederlo@informatik.uni-tuebingen.de>
 *          Bernd Schubert <bernd.schubert@fastmail.fm>
 *
 * Copyright (C) 2007 Q-leap Networks, Goswin von Brederlow
 *               2010 DataDirect Networks, Bernd Schubert
 *               2013 Fraunhofer ITWM, Bernd Schubert
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
 *    along with this program; if not, write_main to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *    USA
 *
 ************************************************************************/

#include "fstest.h"
#include "config.h"

static int stats_interval = 60;
//int size_max = 35; // 32GiB
//int stats_interval = 900;

using namespace std;

/* filesystem constructor */
Filesystem::Filesystem(string dir, size_t percent)
{
	this->last_read_index = 0;

	this->goal_percent = percent;
	pthread_mutex_init(&this->mutex, NULL);
	this->error_detected = false;
	this->terminated = false;

	// Create working dir
	root_dir = new Dir(dir, this);
	memset(&stats_all, 0, sizeof(stats_all));
	this->update_stats(false);

	this->fs_use_goal = (this->fssize * percent) / 100;

	cout << "Filesystem size     : " << this->fssize / KILO << " kiB" << endl;
	cout << "Filesystem free     : " << this->fsfree << endl;
	cout << "Filesystem used     : " << ((this->fsused * 100) / this->fssize) << "%" << endl;
	cout << "Filesystem use-goal : " << this->fs_use_goal / KILO << "kiB" << endl;


	if ( (fssize - fsfree) >= this->fs_use_goal) {
		cerr 	<< "Error: Filesystem already above % used goal: "
			<< this->fsused * 100.0 / fssize << " >= "
			<< goal_percent << endl;
		exit(1);
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

void Filesystem::check_terminate_and_sleep(unsigned seconds)
{
	if (this->terminated)
		pthread_exit(NULL);

	sleep(seconds);
}


/* Update statistics about this filesystem
 * Filesystem has to be locked */
void Filesystem::update_stats(bool size_only)
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
	this->fssize = (uint64_t) statvfsbuf.f_blocks * statvfsbuf.f_frsize;
	this->fsfree = (uint64_t) statvfsbuf.f_bavail * statvfsbuf.f_frsize;

	this->fsused = this->fssize - this->fsfree;

	if (size_only)
		return;

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
	if (this->error_detected || this->terminated)
		pthread_exit(NULL); // Don't delete anything, just exit immediately

	this->update_stats(true);

	int retry_count = 0;

	while(this->fsused + (uint64_t) fsize > this->fs_use_goal
		&& this->files.size() > 2)
	{
		retry_count++;

		if (!this->was_full) {
			this->was_full = true;
			cout << "Going into write/delete mode" << endl;
		}

		// Remove a file

		this->lock();
		int nfiles = this->files.size();
		if (nfiles < 1) {
			this->unlock();
			break;
		}
		int num = random() % nfiles;

		this->file_iter = this->files.begin() + num;
		File *file = *this->file_iter;

		// Don't delete a file that is in read or not checked yet
		// Our read loop does not like that
		if (file->trylock()) {
			// File probably just in read
			this->unlock();
			continue;
		}
		this->unlock();

		string fname = file->fname;
		int nchecks = file->num_checks;

#if 0
		// Check if read-thread already has verified it
		if (nchecks == 0) {
			file->unlock();
			continue;
		}
#endif

		file->set_in_delete();

		// check the file a last time
		if (nchecks < 10) {
			// cout << "num-files: " << this->files.size() <<
			// 	" Unlink check: " << fname << endl;
			if (file->check() ) {
				this->error_detected = true;
				// we exit the write_main thread

				file->unlock();
				pthread_exit(NULL);
			}
			// cout << "Unlink check done: " << fname << endl;
		}

		file->unlock();

		this->lock();

		delete file;
		this->files.erase(this->file_iter);

		this->update_stats(true);

		this->unlock();
	}

#if 0
	if (retry_count)
		cout << "Used after unlink: " << this->fsused / KILO << " kIB" 	<<
			" retryCount: " << retry_count 				<<
			" num files: " << this->files.size() 			<<
			" used + new-file-size: " 				<<
				(this->fsused + (uint64_t) fsize) / KILO << "kiB" <<
			endl;
#endif

}

/** write_main thread
 * when it deletes a file, it will start a read, though
 */
void Filesystem::write_main(void)
{
	int level = 1;
	int max_files = 1;
	new Dir(root_dir, 1);
	ssize_t timeout = get_global_cfg()->get_timeout();

	cout << "Starting test       : " << ctime(&stats_old.time);


	while((this->error_detected == false) && (this->terminated == false)) {
		// cout << "all_dirs: " << all_dirs.size() << endl;
		// cout << "active_dirs: " << active_dirs.size() << endl;

		// Pick a random directory
		int num = random() % active_dirs.size();
		// cout << "Picked " << active_dirs[num]->path() << endl;

		Dir* dir = active_dirs[num];

		// Create file
		File *file = new File(dir);

		// free some space by inDelete a file,
		this->free_space(file->get_fsize() );

		file->fwrite();

		// cout << "Lock file sytem" << endl;
		this->lock(); // LOCK FILESYSTEM

		dir->add_file(file);
		this->files.push_back(file);

		this->stats_now.write += file->get_fsize();
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
			this->update_stats(false);
		}

		// Check if the timeout is reached
		ssize_t elapsed_time = (stats_all.time ? stats_all.time : stats_old.time);
		if ((timeout != -1) && (stats_now.time - elapsed_time > timeout)) {
			cout << "Timeout reached. Now leaving!" << endl;
			this->terminated = true;
		}

		// cout << "UnLock file sytem" << endl;
		this->unlock(); // UNLOCK FILESYSTEM

		// Some filesystems prefer writes over reads. But we don't want
		// to let the reads to fall too far behind. Use arbitrary limit
		// of 20 files
		if (!this->was_full) {
			while (this->last_read_index + 100 < this->files.size())
				check_terminate_and_sleep(1);
		} else {
			while  (this->stats_now.num_written_files > this->stats_now.num_read_files + 20)
				check_terminate_and_sleep(1);
		}
	}

	if (this->error_detected)
		cout << "Error detected, leaving write thread!" << endl;

	pthread_exit(NULL);
}

/* Read from the beginning to the end, if end is reached
 * continue at the beginning
 */
void Filesystem::read_main(void)
{
	unsigned long index = 0;
start_again:
	this->lock();
	// wait until the 2nd file is being written
	while (this->files.size() < 2) {
		this->unlock();
		pthread_yield();
		check_terminate_and_sleep(1);
		goto start_again;
	}

#ifdef DEBUG
	cerr << "Starting to read files" << endl;
#endif

	File *file = this->get_file_locked(index);

	// now lock the file, still a chance of a race, until the unlink()
	// methods also lock the filesystem first -> FIXME
	file->lock();
	this->unlock();

	while(true) {
		// file is locked here
		int fsize = file->get_fsize();

		if (file->is_being_deleted() ) {
			file->unlock();
			goto newindex;
		}

		if (file->check() )
			this->error_detected = true;

		if (this->terminated)
			pthread_exit(NULL);

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

				// give it some time to write_main new data
				check_terminate_and_sleep(1);
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
			// the write_main thread deleted our file...
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
	RETURN(rc);
}


