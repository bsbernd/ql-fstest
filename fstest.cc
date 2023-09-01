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
 *    along with this program; if not, write_main to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *    USA
 *
 ************************************************************************/
#include <execinfo.h>
#include <random>

#include "fstest.h"
#include "config.h"

static Config_fstest global_cfg;

#define BACKTRACE_MAX_SIZE 1024 * 1024
static void* backtrace_buffer[BACKTRACE_MAX_SIZE];



int do_exit(const char* func, const char *file, unsigned line, int code)
{
	int err_fd = fileno(stderr);

	int trace_len = backtrace(backtrace_buffer, BACKTRACE_MAX_SIZE);
	backtrace_symbols_fd(backtrace_buffer, trace_len, err_fd);

	fprintf(stderr, "%s() %s:%d Exit code %d\n", func, file, line, code);
	exit(code);
}

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
	out << cmd << " [options] <dir> - directory on filesystem to test in." << endl;
	out << endl;
	out << "Options:\n";
	out << " -f|--max-files <int>   - maximum number of files created [" <<
			                          global_cfg.get_default_max_files() << "]" << endl;
	out << " -p|--percent <percent> - goal percentage used of filesystem [90]." << endl;
	out << " -t|--timeout <seconds> - goal timeout [-1]." << endl;
	out << " -i|--immediate         - check files immediately after writing them instead of" << endl
	    << "                          letting the read-thread do it later on." << endl;
	out << " --min-bits             - min file size bits 2^n  [20] (1MiB)." << endl;
	out << " --max-bits             - max file size bits 2^n. [30] (1GiB)." << endl;
	out << " --error-stop           - stop on first error instead of further" << endl;
	out << "                          (and endless) checking for more corruptions." << endl;
	out << endl;

}

/* Start the write_main thread here */
void *run_write_thread(void *arg)
{
	Filesystem* t =  (Filesystem *) arg;
	t->write_main();
	return NULL;
}

/* Start the write_main thread here */
void *run_read_thread(void *arg)
{
	Filesystem* t =  (Filesystem *) arg;
	t->read_main();
	return NULL;
}


void start_threads(void)
{
	string dir = global_cfg.get_testdir();
	size_t goal_percent = global_cfg.get_usage();

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

	for (i = 0; i < 2; i++) {
		pthread_join(threads[i], NULL);
		cout << "Thread " << i << " finished" << endl;
	}
}

int main(int argc, char * const argv[])
{
	int res;
	struct stat statbuf;

	cmd = argv[0];

	// Getopt stuff
	const char optstring[] = "f:hip:t:";
	const struct option longopts[] = {
		{ "help"     ,  0, NULL, 'h' },
		{ "error-stop", 0, NULL, 3  },
		{ "immediate",  0, NULL, 'i' },
		{ "percent"  ,  1, NULL, 'p' },
		{ "timeout"  ,  1, NULL, 't' },
		{ "min-bits" ,  1, NULL,  1  },
		{ "max-bits" ,  1, NULL,  2  },
		{ "max-files",  1, NULL, 'f' },
		{ NULL       ,  0, NULL,  0  }
	};
	int longindex = 0;

	// Arguments
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
		case 'f':
			global_cfg.set_max_files(atoi(optarg) );
			break;
		case 'p':
			global_cfg.set_usage(atoi(optarg) );
			break;
		case 't':
			global_cfg.set_timeout(atoi(optarg) );
			break;
		case 1:
			global_cfg.set_min_size_bits(atoi(optarg));
			break;
		case 2: global_cfg.set_max_size_bits(atoi(optarg));
			break;
		case 3:
			global_cfg.set_error_immediate_stop();
			break;
		default:
			fprintf (stderr, "Error: unknown option '%c'\n", res);
			usage(cerr);
			exit(1);
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
		exit(1);
	}

	// Check that testdir exists and is a directory
	if (testdir.length() == 0) {
		cerr << "Error: " << "please specify test directory\n";
		exit(1);
	}

	if (stat(testdir.c_str(), &statbuf) != 0) {
		perror(testdir.c_str());
		exit(1);
	}

	if (!S_ISDIR(statbuf.st_mode)) {
		cerr << "Error: " << testdir << " is not a directory\n";
		exit(1);
	}

	if (global_cfg.get_max_files() < QL_FSTEST_MIN_NUM_FILES)
	{
		cerr <<  "Max files is too small: "
			 << global_cfg.get_max_files() << " vs. "
			 << QL_FSTEST_MIN_NUM_FILES << std::endl;
		exit(1);
	}


	global_cfg.set_testdir(testdir);

	cout << "fstest v0.1\n";
	cout << "Directory           : " << testdir << endl;

	start_threads();

	cout << "Done.\n";
	RETURN(0);
}
