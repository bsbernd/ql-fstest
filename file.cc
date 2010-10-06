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
#include "file.h"

const char HEX[] = "0123456789ABCDEF";
const size_t BUF_SIZE = 1024*1024; // Must be power of 2

using namespace std;

File::File(Dir *dir, uint64_t num)
{
	directory = dir;
	prev = NULL;
	next = NULL;
	fsize = num;
	
	int fd;

	string path = dir->path();
	
	dir->add_file(this);
	// Create file
retry:
	id = random();
	cout << "id: " << id << " Char: ";
	snprintf(fname, 9, "%x", id);

	fd = open((path + fname).c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd == -1) {
		if (errno == EEXIST)
			goto retry; // Try again with new name
		std::cout << "Creating file " << path << fname;
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
	for(s = 0; s < fsize; s += BUF_SIZE) {
		size_t offset = 0;
		while(offset < BUF_SIZE) {
			ssize_t len;
			// std::cout << "Write " << path << fname << " at " << s << std::endl;
			if ((len = write(fd, &buf[offset], BUF_SIZE - offset)) < 0) {
				std::cerr << "Write to " << path << fname << " failed";
				perror(": ");
				EXIT(1);
			}
			offset += len;
		}
	}

	fdatasync(fd);
	// Try to remove pages from memory to let the kernel re-read the file
	// on later reads
	posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
	close(fd);
	directory->fs->files.push_back(this);
}

File::~File(void)
{
	// std::cout << "~File(" << dir->path() + fname << ")\n";
	// Remove from dir
	directory->remove_file(this);
	// delete file
	if (::unlink((directory->path() + fname).c_str()) != 0)
	{
		std::cerr << "Deleting file " << directory->path() << fname << " failed";
		perror(": ");
		EXIT(1);
	}
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

void File::check(void)
{
	int fd;
	// Open file
	if ((fd = open((directory->path() + fname).c_str(), O_RDONLY)) == -1) {
		std::cerr << "Checking file " << directory->path() << fname;
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
				std::cerr << "Read from " << directory->path() 
					  << fname << " failed";
				perror(": ");
				EXIT(1);
			}
			offset += len;
		}
		if (memcmp(bufm, buff, BUF_SIZE) != 0) {
			std::cerr << "File corruption in " 
				  << directory->path() << fname
			          << " around " << s << " [pattern = "
			          << std::hex << id << std::dec << "]\n";
			for (unsigned ia = 0; ia < BUF_SIZE; ia++) {
				if (memcmp(bufm + ia, buff + ia, 1) != 0) {
					fprintf(stderr, "Expected: %x, got: %x (pos = %ld)\n",
					        (unsigned char) bufm[ia], (unsigned char) buff[ia],
					        s + ia);
					EXIT (1);
				}
			}
			// we actually never should get here
			std::cerr << "Bug detected, previous exit never reached\n";
			EXIT (1);
		}
	}

	// Try to remove pages from memory to let the kernel re-read the file
	// on later reads
	posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);

	close(fd);
}

File * File::get_next() const
{
	return next;
}

size_t File::get_fsize() const
{
	return fsize;
}

