/*
 * Configuration options for the fstest utility
 *
 *    Copyright (C) 2013 Fraunhofer ITWM, Bernd Schubert
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

#ifndef CONFIG_H_
#define CONFIG_H_

#define MAX_USAGE_PERCENT 90 // fill file system up to this level
#define TIMEOUT -1 // timeout before leaving

// file sizes between min and max
#define DEFAULT_MIN_SIZE_BITS 20 // 2^20 = 1MiB
#define DEFAULT_MAX_SIZE_BITS 30 // 2^30 = 1GiB


class Config_fstest {
public:
	Config_fstest(void)
	{
		this->usage_percent = MAX_USAGE_PERCENT;
		this->timeout       = TIMEOUT;
		this->immediate_check = false;
		this->testdir = "";
		this->min_size_bits = DEFAULT_MIN_SIZE_BITS;
		this->max_size_bits = DEFAULT_MAX_SIZE_BITS;
		this->error_stop = false;
		this->max_files = QL_FSTEST_DEFAULT_NUM_FILES;
		this->direct_io = false;
	}

private:
	size_t usage_percent; // max fill level
	ssize_t timeout; // in seconds
	bool immediate_check;
	string testdir;
	size_t min_size_bits;
	size_t max_size_bits;
	size_t max_files;
	bool error_stop; // stop on first error
	bool direct_io;

public:
	void set_usage(size_t value)
	{
		this->usage_percent = value;
	}

	size_t get_usage(void)
	{
		return this->usage_percent;
	}

	void set_timeout(ssize_t value)
	{
		this->timeout = value;
	}

	ssize_t get_timeout(void)
	{
		return this->timeout;
	}

	void set_immediate_check(bool value)
	{
		this->immediate_check = value;
	}

	bool get_immediate_check(void)
	{
		return this->immediate_check;
	}

	void set_testdir(string testdir)
	{
		if ((testdir.length() > 0)
		&& (testdir.at(testdir.length() - 1) != '/')) {
			testdir += '/';
		}

		// add pid to directory
		pid_t pid = getpid();
		stringstream str;
		str << pid;
		testdir += "fstest." + str.str() + "/";

		this->testdir = testdir;
	}

	string get_testdir(void)
	{
		return this->testdir;
	}

	void set_min_size_bits(int min)
	{
		this->min_size_bits = min;
	}

	int get_min_size_bits(void)
	{
		return this->min_size_bits;
	}

	void set_max_size_bits(size_t max)
	{
		this->max_size_bits = max;
	}

	size_t get_max_size_bits(void)
	{
		return this->max_size_bits;
	}

	void set_max_files(size_t max_files)
	{
		this->max_files = max_files;
	}

	size_t get_max_files(void)
	{
		return this->max_files;
	}

	ssize_t get_default_max_files(void)
	{
		return QL_FSTEST_DEFAULT_NUM_FILES;
	}

	void set_error_immediate_stop(void)
	{
		this->error_stop = true;
	}

	bool get_error_immediate_stop()
	{
		return this->error_stop;
	}

	void set_direct_io(void)
	{
		this->direct_io = true;
	}

	bool get_direct_io(void)
	{
		return this->direct_io;
	}

};

Config_fstest *get_global_cfg(void);

#endif /* CONFIG_H_ */
