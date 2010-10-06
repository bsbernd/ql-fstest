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

int size_min = 20; // 1MiB
int size_max = 30; // 1GiB
int stats_interval = 60;
//int size_max = 35; // 32GiB
//int stats_interval = 900;


using namespace std;

static char *cmd;

void usage(ostream &out)
{
	out << "\n";
	out << cmd << " -h|--help         - show help.\n";
	out << cmd << " [options] [<dir>] - directory on filesystem to test in.\n";
	out << endl;
	out << "Options:\n";
	out << " -p|--percent <percent> - goal percentage used of filesystem [90].\n";
	out << "\n";
	
}

/* filesystem constructor */
Filesystem::Filesystem(string dir, double percent)
{
	goal_percent = percent;
	
	// Create working dir
	root_dir = new Dir(dir, this);
	this->update_stats();
	
	cout << "Filesystem size     : " << fssize << endl;
	cout << "Filesystem free     : " << fsfree << endl;
	cout << "Filesystem used     : " << 100 * double(fssize - fsfree) / fssize << endl;
	if (double(fssize - fsfree) / fssize >= goal_percent) {
		cerr 	<< "Error: Filesystem already above % used goal: " 
			<< double(fssize - fsfree) / fssize << " >= " 
			<< goal_percent << endl;
		EXIT(1);
	}
	
	memset(&stats_old, 0, sizeof(stats_old));
	memset(&stats_now, 0, sizeof(stats_now));
	
	stats_old.time = time(NULL);
	stats_now.time = time(NULL);
	
	was_full = false;
}

Filesystem::~Filesystem(void)
{
	if (root_dir != NULL) {
		delete root_dir;
		root_dir = NULL;
	}
}



/* Update statistics about this filesystem */
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
}


/**
 * free some disk space if usage above goal
 */
void Filesystem::free_space(size_t fsize)
{
	// Check space
	while(true)
	{
		struct statvfs vfsbuf;
		if (statvfs(root_dir->path().c_str(), &vfsbuf) != 0) {
			perror("statvfs()");
			EXIT(1);
		}
		uint64_t fsfree = vfsbuf.f_bavail * vfsbuf.f_frsize;
		// cout << "Free: " << fsfree << endl;
		// cout << "Goal: " << goal << endl;
		// Enough free already?
		if (fsize < fsfree && fsfree - fsize / 2 > goal) {
			// remove 1 file in 2 untill the fs was full once
			if (was_full || stats_now.num_files > files.size() * 2)
				break;
		} else {
			was_full = true;
		}
		// Remove some files
		if (files.size() == 0)
			break; // no files left to delete
		int num = random() % files.size();
		files[num]->check();
		stats_now.read += files[num]->get_fsize();
		delete files[num];
		files[num] = files[files.size() - 1];
		files.resize(files.size() - 1);
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

	while(true) {
		// cout << "all_dirs: " << all_dirs.size() << endl;
		// cout << "active_dirs: " << active_dirs.size() << endl;

		// Pick a random file size
		size_t fsize = 1ULL << (size_min + random() % (size_max - size_min + 1));
		// cout << "Size: " << size << endl;

		this->free_space(fsize);

		// Pick a random directory
		int num = random() % active_dirs.size();
		// cout << "Picked " << active_dirs[num]->path() << endl;

		// Create file
		new File(active_dirs[num], fsize);
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
			stats_old = stats_now;
		}
	}
}

int main(int argc, char * const argv[])
{
	int res;
	struct stat statbuf;

	cmd = argv[0];

	// Getopt stuff
	const char optstring[] = "hp:";
	const struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ "percent", 1, NULL, 'p' },
		{ NULL, 0, NULL, 0 }
	};
	int longindex = 0;

	// Arguments
	double percent = 0.9;
	string dir = "";

	while((res = getopt_long(argc, argv, optstring, longopts, &longindex)) != -1) {
		switch(res) {
		case 'h':
			usage(cout);
			exit(0);
			break;
		case '?':
			usage(cerr);
			EXIT(1);
			break;
		case 'p':
			percent = strtod(optarg, NULL) / 100;
			break;
		default:
			printf ("Error: unknown option '%c'\n", res);
			usage(cerr);
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
		cerr << "Error: extra args:";
		for(i = optind; i < argc; ++i) {
			cerr << " '" << argv[i] << "'";
		}
		cerr << endl;
		usage(cerr);
		EXIT(1);
	}

	// Check that dir exists and is a directory
	if (dir.length() > 0) {
		if (stat(dir.c_str(), &statbuf) != 0) {
			perror(dir.c_str());
			EXIT(1);
		}
		if (!S_ISDIR(statbuf.st_mode)) {
			cerr << "Error: " << dir << " is not a directory.\n";
			EXIT(1);
		}
	}

	{
		// add pid to directory
		pid_t pid = getpid();
		stringstream str;
		str << pid;
		dir += "fstest." + str.str() + "/";
	}

	cout << "fstest v0.0\n";
	cout << "Directory           : " << ((dir=="")?"./":dir) << endl;
	cout << "Goal percentage used: " << percent * 100 << endl;

	Filesystem * filesystem = new Filesystem(dir, percent);
	filesystem->write();

	cout << "Done.\n";
	return 0;
}
