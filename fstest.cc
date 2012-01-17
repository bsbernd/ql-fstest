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
#include "config.h"

static Config_fstest global_cfg;

/**
 * Return the program wide configuration
 */
Config_fstest *get_global_cfg(void)
{
   return &global_cfg;
}


using namespace std;

static char *cmd;

void usage(ostream &out)
{
	out << endl;
	out << cmd << " -h|--help         - show help." << endl;
	out << cmd << " [options] [<dir>] - directory on filesystem to test in." << endl;
	out << endl;
	out << "Options:\n";
	out << " -p|--percent <percent> - goal percentage used of filesystem [90]." << endl;
	out << " -i|--immediate         - check files immediately after writing them instead of" << endl
	    << "                          letting the read-thread do it later on." << endl;
	out << endl;
	
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


void start_threads(void)
{
	string dir = global_cfg.get_testdir();
	double goal_percent = global_cfg.get_usage();

	Filesystem * filesystem = new Filesystem(dir, goal_percent);
	
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
	const char optstring[] = "hip:";
	const struct option longopts[] = {
		{ "help", 0, NULL, 'h' },
		{ "immediate", 0, NULL, 'i' },
		{ "percent", 1, NULL, 'p' },
		{ NULL, 0, NULL, 0 }
	};
	int longindex = 0;

	// Arguments
	double percent = 0.9;
	string testdir = "";

	while((res = getopt_long(argc, argv, optstring, longopts, &longindex)) != -1) {
		switch(res) {
		case 'h':
			usage(cout);
			exit(0);
			break;
		case 'i':
			global_cfg.set_immediate_check(true);
			break;
		case '?':
			usage(cerr);
			EXIT(1);
			break;
		case 'p':
			global_cfg.set_usage(strtod(optarg, NULL) / 100);
			break;
		default:
			printf ("Error: unknown option '%c'\n", res);
			usage(cerr);
			EXIT(1);
		}
	}

	// Get optional arg <testdir> and add trailing '/'
	if (optind < argc) {
		testdir = argv[optind++];
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

	// Check that testdir exists and is a directory
	if (testdir.length() > 0) {
		if (stat(testdir.c_str(), &statbuf) != 0) {
			perror(testdir.c_str());
			EXIT(1);
		}
		if (!S_ISDIR(statbuf.st_mode)) {
			cerr << "Error: " << testdir << " is not a directory.\n";
			EXIT(1);
		}
	}

	global_cfg.set_testdir(testdir);

	cout << "fstest v0.1\n";
	cout << "Directory           : " << ((testdir == "" ) ? "./" : testdir) << endl;
	cout << "Goal percentage used: " << percent * 100 << endl;

	start_threads();

	cout << "Done.\n";
	RETURN(0);
}
