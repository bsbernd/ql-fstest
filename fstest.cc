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

/* Start the write thread here */
void *run_write_thread(void *arg)
{
	Filesystem* t =  (Filesystem *) arg;
	t->write();
	return NULL;
}

/* Start the write thread here */
void *run_read_thread(void *arg)
{
	Filesystem* t =  (Filesystem *) arg;
	t->read_loop();
	return NULL;
}


void start_threads(string dir, double percent)
{
	Filesystem * filesystem = new Filesystem(dir, percent);
	
	int rc;
	pthread_t threads[2];
	// struct pthread_arg tinfo[2];
	
	
	// FIXME: We need a pthread wrapper class, our current way is ugly
	
	int i = 0;
	rc = pthread_create(&threads[i], NULL, run_write_thread, filesystem);
	if (rc) {
		cerr << "Failed to start thread " << i << ": " 
			<< strerror(rc) << endl;
		EXIT(1);
	}
	
	i = 1;
	rc = pthread_create(&threads[i], NULL, run_read_thread, filesystem);
	if (rc) {
		cerr << "Failed to start thread " << i << ": " 
			<< strerror(rc) << endl;
		EXIT(1);
	}
	
	for (i = 0; i < 2; i++)
		pthread_join(threads[i], NULL);
		cout << "Thread " << i << "finished" << endl;
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

	cout << "fstest v0.1\n";
	cout << "Directory           : " << ((dir=="")?"./":dir) << endl;
	cout << "Goal percentage used: " << percent * 100 << endl;

	start_threads(dir, percent);

	cout << "Done.\n";
	return 0;
}
