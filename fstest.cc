/************************************************************************ 
 * 
 * Filesystem stress and verify
 * 
 * Authors: Goswin von Brederlow <brederlo@informatik.uni-tuebingen.de>
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

static int do_exit(const char* func, unsigned line, int code)
{
	fprintf(stderr, "%s:%d Exit code %d\n", func, line, code);
	exit(code);
}

#define EXIT(x) do_exit(__func__, __LINE__, x)

// Global options
int size_min = 20; // 1MiB
int size_max = 30; // 1GiB
int stats_interval = 60;
//int size_max = 35; // 32GiB
//int stats_interval = 900;

const char HEX[] = "0123456789ABCDEF";
const size_t BUF_SIZE = 1024*1024; // Must be power of 2
const uint64_t MEGA = 1024 * 1024;
const uint64_t GIGA = 1024 * 1024 * 1024;

class File;
class Dir;

std::vector<Dir*> all_dirs;
std::vector<Dir*> active_dirs;
std::vector<File*> files;
uint64_t fssize = 0;
Dir *root_dir = NULL;
char *cmd = NULL;

struct TimeStamp {
  time_t time;
  uint64_t write, read, files;
};

class Dir {
public:
  Dir(Dir *parent, int num) : parent(parent), next(parent->sub), sub(NULL), files(NULL), name((char[]){'d', '0' + num / 10, '0' + num % 10, 0}), num_files(0) {
    std::cout << "Creating dir " << parent->path() << name << std::endl;
    parent->sub = this;
    if (mkdir(path().c_str(), 0700) != 0) {
      std::cout << "Creating dir " << path();
      perror(": ");
      EXIT(1);
    }
    all_dirs.push_back(this);
    active_dirs.push_back(this);
    while(--num > 0) {
      new Dir(this, num);
    }
  }
  Dir(std::string _path) : parent(NULL), next(NULL), sub(NULL), files(NULL), name((char[]){'r', 'o', 't', 0}), num_files(1) {
    root_path = _path;
    if (mkdir(path().c_str(), 0700) != 0) {
      std::cout << "Creating working dir " << path();
      perror(": ");
      EXIT(1);
    }
    all_dirs.push_back(this);
    active_dirs.push_back(this);
  }
  ~Dir();
  std::string path() const {
    if (parent == NULL) {
      return root_path;
    } else {
      return parent->path() + name + "/";
    }
  }
  void add_file(File *file);
  void remove_file(File *file);
  uint16_t get_num_files() const { return num_files; }
private:
  Dir *parent;
  Dir *next;
  Dir *sub;
  File *files;
  char name[4];
  uint16_t num_files;
  static std::string root_path;
};
std::string Dir::root_path;

class File {
public:
  File(Dir *dir, uint64_t size) : dir(dir), prev(NULL), next(NULL), size(size) {
    //std::cout << "File('" << dir->path() << "', " << size << ")\n";
    int fd;
    int i;
    uint32_t t;
    std::string path = dir->path();

    dir->add_file(this);

    // Create file
  retry:
    id = t = random();
    for(i = 0; i < 8; ++i) {
      name[i^1] = HEX[t % 16];
      t /= 16;
    }
    name[8] = 0;

    if ((fd = open((path + name).c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600)) == -1) {
      if (errno == EEXIST) goto retry; // Try again with new name
      std::cout << "Creating file " << path << name;
      perror(": ");
      EXIT(1);
    }
    // std::cout << "Created " << path << name << std::endl;

    // Create buffer and fill with id
    char buf[BUF_SIZE];
    ((uint32_t*)buf)[0] = id;
    size_t s = sizeof(id);
    while(s < BUF_SIZE) {
      memcpy(&buf[s], &buf[0], s);
      s *= 2;
    }

    // write file
    for(s = 0; s < size; s += BUF_SIZE) {
      size_t offset = 0;
      while(offset < BUF_SIZE) {
	ssize_t len;
	// std::cout << "Write " << path << name << " at " << s << std::endl;
	if ((len = write(fd, &buf[offset], BUF_SIZE - offset)) < 0) {
	  std::cerr << "Write to " << path << name << " failed";
	  perror(": ");
	  EXIT(1);
	}
	offset += len;
      }
    }

    fdatasync(fd);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    close(fd);
    files.push_back(this);
  }
  ~File() {
    // std::cout << "~File(" << dir->path() + name << ")\n";
    // Remove from dir
    dir->remove_file(this);
    // delete file
    if (::unlink((dir->path() + name).c_str()) != 0) {
      std::cerr << "Deleting file " << dir->path() << name << " failed";
      perror(": ");
      EXIT(1);
    }
  }
  void delete_all() {
    File *p = this;
    while(p != NULL) {
      File *n = p->next;
      delete p;
      p = n;
    }
  }
  void link(File *file) {
    next = file;
    if (next != NULL) {
      prev = next->prev;
      next->prev = this;
    }
    if (prev != NULL) prev->next = this;
  }
  void unlink() {
    if (prev != NULL) prev->next = next;
    if (next != NULL) next->prev = prev;
    prev = next = NULL;
  }
  void check() {
    //std::cout << "File::check() on " << dir->path() << name << ", size " << size << std::endl;
    int fd;

    // Open file
    if ((fd = open((dir->path() + name).c_str(), O_RDONLY)) == -1) {
      std::cerr << "Checking file " << dir->path() << name;
      perror(": ");
      EXIT(1);
    }

    // Don't call it, as we want to create some cache pressure to stress
    // the VFS
    // posix_fadvise(fd,0 ,0, POSIX_FADV_NOREUSE);

    // Create buffer and fill with id
    char bufm[BUF_SIZE];
    char buff[BUF_SIZE];
    ((uint32_t*)bufm)[0] = id;
    size_t s = sizeof(id);
    while(s < BUF_SIZE) {
      memcpy(&bufm[s], &bufm[0], s);
      s *= 2;
    }

    // read and compare file
	for(s = 0; s < size; s += BUF_SIZE) {
		size_t offset = 0;
		while(offset < BUF_SIZE) {
			ssize_t len;
			if ((len = read(fd, &buff[offset], BUF_SIZE - offset)) < 0) {
				std::cerr << "Read from " << dir->path() << name << " failed";
				perror(": ");
				EXIT(1);
			}
			offset += len;
		}
		if (memcmp(bufm, buff, BUF_SIZE) != 0) {
			std::cerr << "File corruption in " << dir->path() << name
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

    close(fd);
  }
  File *get_next() const { return next; }
  size_t get_size() const { return size; }
private:
  Dir *dir;
  File *prev, *next;
  uint64_t size;
  uint32_t id;
  char name[9];
};

Dir::~Dir() {
    std::cout << "~Dir(" << path() << ")\n";
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

void Dir::add_file(File *file) {
  file->link(files);
  files = file;
  ++num_files;
}

void Dir::remove_file(File *file) {
  --num_files;
  if (files == file) files = files->get_next();
  file->unlink();
}

void usage(std::ostream &out) {
    out << cmd << " -h|--help         - show help.\n";
    out << cmd << " [options] [<dir>] - directory on filesystem to test in.\n";
    out << std::endl;
    out << "Options:\n";
    out << " -p|--percent <percent> - goal percentage used of filesystem [90].\n";
}

void cleandir(void) {
    std::cout << "cleandir()\n";
    if (root_dir != NULL) {
	delete root_dir;
	root_dir = NULL;
    }
}

void fstest(uint64_t goal) {
  TimeStamp old = { time(NULL), 0, 0, 0}, now = { time(NULL), 0, 0, 0};
  int level = 1;
  int max_files = 1;
  bool was_full = false;
  new Dir(root_dir, 1);
  std::cout << "Starting test       : " << ctime(&old.time);

  while(true) {
    // std::cout << "all_dirs: " << all_dirs.size() << std::endl;
    // std::cout << "active_dirs: " << active_dirs.size() << std::endl;
    
    // Pick a random file size
    size_t size = 1ULL << (size_min + random() % (size_max - size_min + 1));
    // std::cout << "Size: " << size << std::endl;

    // Check space
    while(true) {
      struct statvfs vfsbuf;
      if (statvfs(root_dir->path().c_str(), &vfsbuf) != 0) {
	perror("statvfs()");
	EXIT(1);
      }
      uint64_t fsfree = vfsbuf.f_bavail * vfsbuf.f_frsize;
      // std::cout << "Free: " << fsfree << std::endl;
      // std::cout << "Goal: " << goal << std::endl;

      // Enough free already?
      if (size < fsfree && fsfree - size / 2 > goal) {
	  // remove 1 file in 2 untill the fs was full once
	  if (was_full || now.files > files.size() * 2) break;
      } else {
	  was_full = true;
      }

      // Remove some files
      if (files.size() == 0) break; // no files left to delete
      int num = random() % files.size();
      files[num]->check();
      now.read += files[num]->get_size();
      delete files[num];
      files[num] = files[files.size() - 1];
      files.resize(files.size() - 1);
    }
    
    // Pick a random directory
    int num = random() % active_dirs.size();
    // std::cout << "Picked " << active_dirs[num]->path() << std::endl;

    // Create file
    new File(active_dirs[num], size);
    now.write += size;
    ++now.files;
    // std::cout << "dir->num_files: " << active_dirs[num]->get_num_files() << std::endl;
    // std::cout << "files: " << files.size() << std::endl;

    // Remove dir from active_dirs if full
    if (active_dirs[num]->get_num_files() >= max_files) {
      active_dirs[num] = active_dirs[active_dirs.size() - 1];
      active_dirs.resize(active_dirs.size() - 1);
    }

    if (active_dirs.size() == 0) {
      ++level;
      max_files = level * level;
      std::cout << "Going to level " << level << std::endl;
      active_dirs = all_dirs;
      new Dir(root_dir, level);
    };

    // Output stats
    now.time = time(NULL);
    if (now.time - old.time > stats_interval) {
      double t = now.time - old.time;
      double write = (now.write - old.write) / t / MEGA;
      double read = (now.read - old.read) / t / MEGA;
      double files = (now.files - old.files) / t;
      std::cout << now.time << " write: " << now.write / GIGA << " GiB [" << write << " MiB/s] read: " << now.read / GIGA << " GiB [" << read << " MiB/s] Files: " << now.files << " [" << files << " files/s] # " << ctime(&now.time);
      std::cout.flush();
      old = now;
    }
  }
}

int main(int argc, char * const argv[]) {
    int res;
    struct stat statbuf;
    struct statvfs statvfsbuf;

    cmd = argv[0];

    // Getopt stuff
    const char optstring[] = "hp:";
    const struct option longopts[] = {
	{ "help", 0, NULL, 'h' },
	{ "percent", 1, NULL, 'p' },
	{ NULL, 0, NULL, 0 }};
    int longindex = 0;

    // Arguments
    double percent = 0.9;
    std::string dir = "";

    while((res = getopt_long(argc, argv, optstring, longopts, &longindex)) != -1) {
	switch(res) {
	case 'h':
	    usage(std::cout);
	    EXIT(0);
	    break;
	case '?':
	    usage(std::cerr);
	    EXIT(1);
	    break;
	case 'p':
	    percent = strtod(optarg, NULL) / 100;
	    break;
	default:
	    printf ("Error: unknown option '%c'\n", res);
	    usage(std::cerr);
	    EXIT(1);
	}
    }

    // Get optional arg <dir> and add trailing '/'
    if (optind < argc) {
	dir = argv[optind++];
	if ((dir.length() > 0) && (dir[dir.length() - 1] != '/')) {
	    dir += '/';
	}
    }

    if (optind < argc) {
	int i;
	std::cerr << "Error: extra args:";
	for(i = optind; i < argc; ++i) {
	    std::cerr << " '" << argv[i] << "'";
	}
	std::cerr << std::endl;
	usage(std::cerr);
	EXIT(1);
    }

    // Check that dir exists and is a directory
    if (dir.length() > 0) {
	if (stat(dir.c_str(), &statbuf) != 0) {
	    perror(dir.c_str());
	    EXIT(1);
	}
	if (!S_ISDIR(statbuf.st_mode)) {
	    std::cerr << "Error: " << dir << " is not a directory.\n";
	    EXIT(1);
	}
    }

    { // add pid to directory
	pid_t pid = getpid();
	std::stringstream str;
	str << pid;
	dir += "fstest." + str.str() + "/";
    }

    std::cout << "fstest v0.0\n";
    std::cout << "Directory           : " << ((dir=="")?"./":dir) << std::endl;
    std::cout << "Goal percentage used: " << percent * 100 << std::endl;

    // Create working dir
    root_dir = new Dir(dir);

    // Get FS stats
    if (statvfs(root_dir->path().c_str(), &statvfsbuf) != 0) {
	perror("statvfs(): ");
	EXIT(1);
    }
    fssize = statvfsbuf.f_blocks * statvfsbuf.f_frsize;
    uint64_t fsfree = statvfsbuf.f_bavail * statvfsbuf.f_frsize;
    std::cout << "Filesystem size     : " << fssize << std::endl;
    std::cout << "Filesystem free     : " << fsfree << std::endl;
    std::cout << "Filesystem used     : " << 100 * double(fssize - fsfree) / fssize << std::endl;
    if (double(fssize - fsfree) / fssize >= percent) {
	std::cerr << "Error: Filesystem already above % used goal: " << double(fssize - fsfree) / fssize << " >= " << percent << std::endl;
	EXIT(1);
    }

    fstest(uint64_t(fssize * (1 - percent)));

    std::cout << "Done.\n";
    return 0;
}
