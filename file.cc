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

#include "fstest.h"
#include "file.h"

const char HEX[] = "0123456789ABCDEF";
const size_t BUF_SIZE = 1024*1024; // Must be power of 2

using namespace std;

File::File(Dir *dir, size_t fsize) : fsize(fsize)
{
	directory = dir;
	prev = NULL;
	next = NULL;
	num_checks = 0;
	has_error = false;
	pthread_mutex_init(&this->mutex, NULL);
	
	// No need to lock the file here, as it is not globally known yet 
	// Initialization is just suffcient
	
	int fd;
	string path = dir->path();
	
	dir->add_file(this);
	// Create file
retry:
	this->id = random();
	snprintf(fname, 9, "%x", id);

	fd = open((path + this->fname).c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd == -1) {
		if (errno == EEXIST)
			goto retry; // Try again with new name
		std::cout << "Creating file " << path << fname;
		perror(": ");
		EXIT(1);
	}
	
	close(fd);
	
	directory->fs->files.push_back(this);
}

/* Write a file here 
 * file needs to be locked already 
 */
void File::fwrite(void)
{
	int fd;
	string path = directory->path();

	fd = open((path + this->fname).c_str(), O_WRONLY);
	if (fd == -1) {
		std::cerr << "Writing file " << path << fname;
		perror(": ");
		EXIT(1);
	}
	
	// Create buffer and fill with id
	char buf[BUF_SIZE];
	((uint32_t*)buf)[0] = id;
	size_t s = sizeof(id);
	while(s < BUF_SIZE) {
		memcpy(&buf[s], &buf[0], s);
		s *= 2;
	}
	// write file
	for(s = 0; s < this->fsize; s += BUF_SIZE) {
		size_t offset = 0;
		while(offset < BUF_SIZE) {
			ssize_t len;
#ifdef DEBUG
			cout << "Write " << path << fname << " at " << s << std::endl;
#endif
			if ((len = write(fd, &buf[offset], BUF_SIZE - offset)) < 0) {
				if (errno == ENOSPC) {
					cout << path << fname 
						<< ": Out of disk space, "
						<< "probably a race with another thread" << endl;
					goto out;
				}
				if(errno == EIO) {
					cout << "A Lustre eviction?" << endl;
					goto out;
				}
				cerr << "Write to " << path << fname << " failed";
				perror(": ");
				EXIT(1);
			}
			offset += len;
		}
	}

out:
	fdatasync(fd);
	// Try to remove pages from memory to let the kernel re-read the file
	// from disk on later reads
	posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
	close(fd);
}


/* file destructor - delete a file
 * the filesystem has to be locked before calling this
 */
File::~File(void)
{
#ifdef DEBUG
	cout << "~File(" << this->directory->path() + this->fname << ")" << endl;
#endif

	if (this->has_error) {
		cout << "Refusing to delete " 
			<< this->directory->path() + this->fname << endl;
		this->unlock();
		return;
	}

	// Remove from dir
	directory->remove_file(this);
	// delete file
	if (::unlink((directory->path() + fname).c_str()) != 0)
	{
		cerr << "Deleting file " << directory->path() << fname << " failed";
		perror(": ");
		EXIT(1);
	}
	this->unlock();
	pthread_mutex_destroy(&this->mutex);
}

void File::delete_all(void)
{
	File *p = this;
	while(p != NULL) {
		File *n = p->next;
		delete p;
		p = n;
	}
}

void File::link(File *file)
{
	next = file;
	if (next != NULL) {
		prev = next->prev;
		next->prev = this;
	}
	if (prev != NULL) prev->next = this;
}

void File::unlink()
{
	if (prev != NULL) prev->next = next;
	if (next != NULL) next->prev = prev;
	prev = next = NULL;
}

/* check the file for corruption
 * the file MUST be locked before calling this method
 */
int File::check(void)
{
#ifdef DEBUG
	cerr << " Checking file " << this->directory->path() << this->fname << endl;
#endif

	if (this->has_error)
		return 0; // No need to further check this

	if (this->trylock() != EBUSY)
		cout << "Program error:  file is not locked " << this->fname << endl;
	
	int fd;
	// Open file
again:
	fd = open((directory->path() + fname).c_str(), O_RDONLY);
	if (fd == -1) {
		cerr << " Checking file " << this->directory->path() << fname;
		perror(": ");
		EXIT(1);
	}

	// Do not keep the pages in memory, later checks then have to re-read it.
	// Disadvantage is that we do not create memory pressure then, which is
	// usually good to stress test filesystems
	posix_fadvise(fd, 0 ,0, POSIX_FADV_NOREUSE);

	//Create buffer and fill with id
	char bufm[BUF_SIZE];
	char buff[BUF_SIZE];
	((uint32_t*)bufm)[0] = id;
	size_t s = sizeof(id);
	while(s < BUF_SIZE) {
		memcpy(&bufm[s], &bufm[0], s);
		s *= 2;
	}
	// read and compare file
	for(s = 0; s < fsize; s += BUF_SIZE) {
		size_t offset = 0;
		while(offset < BUF_SIZE) {
			ssize_t len;
			len = read(fd, &buff[offset], BUF_SIZE - offset);
			if (len < 0) {
				if(errno == EIO) {
					cout << "IO error. A Lustre eviction?" << endl;
					posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
					close(fd);
					goto again;
				}
				cerr << "Read from " << directory->path() 
					  << fname << " failed";
				perror(": ");
				EXIT(1);
			}
			offset += len;
		}

		// If the filesystem was full, not the complete file was written
		// and so the file might not have a size being a multiple of
		// BUF_SIZE. So introduce a cmp size.
		size_t cmpsize;
		cmpsize = max(BUF_SIZE, fsize - s);
		if (memcmp(bufm, buff, cmpsize) != 0) {
			this->has_error = true;
			cerr << "File corruption in " 
				<< directory->path() << fname
			        << " around " << s << " [pattern = "
			        << std::hex << id << std::dec << "]" << endl;
			cerr << "After n-checks: " <<  this->num_checks << endl;
			for (unsigned ia = 0; ia < BUF_SIZE; ia++) {
				if (memcmp(bufm + ia, buff + ia, 1) != 0) {
					fprintf(stderr, "Expected: %x, got: %x (pos = %ld)\n",
					        (unsigned char) bufm[ia], (unsigned char) buff[ia],
					        s + ia);
				}
			}
			return 1;
		}
	}

	// Try to remove pages from memory to let the kernel re-read the file
	// on later reads
	posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);

	close(fd);
	
	this->num_checks++;
	return 0;
}

File * File::get_next() const
{
	return this->next;
}

size_t File::get_fsize() const
{
	return fsize;
}

void File::lock(void)
{
	int rc = pthread_mutex_lock(&this->mutex);
	if (rc) {
		cerr << "Failed to lock " << this->fname << " : " << strerror(rc);
		perror(": ");
		EXIT(1);
	}
}

void File::unlock(void)
{
	int rc = pthread_mutex_unlock(&this->mutex);
	if (rc) {
		cerr << "Failed to lock " << this->fname << " : " << strerror(rc);
		perror(": ");
		EXIT(1);
	}
}

int File::trylock(void)
{
	int rc = pthread_mutex_trylock(&this->mutex);
	if (rc && rc != EBUSY) {
		cerr << "Failed to lock " << this->fname << " : " << strerror(rc);
		perror(": ");
		EXIT(1);
	}
	return rc;
}

