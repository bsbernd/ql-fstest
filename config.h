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

// file sizes between min and max
#define DEFAULT_MIN_SIZE_BITS 20 // 2^20 = 1MiB
#define DEFAULT MAX_SIZE_BITS 30 // 2^30 = 1GiB


class Config_fstest {
public:
	Config_fstest(void)
	{
		this->usage_percent = MAX_USAGE_PERCENT;
		this->immediate_check = false;
		this->testdir = "";
		this->min_size_bits = 20; 
		this->max_size_bits = 30; 
	}

private:
	double usage_percent; // max fill level
	bool immediate_check;
	string testdir;
	int min_size_bits;
	int max_size_bits;

public:
	void set_usage(double value)
	{
		this->usage_percent = value;
	}

	double get_usage(void)
	{
		return this->usage_percent;
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

	void set_max_size_bits(int max)
	{
		this->max_size_bits = max;
	}

	int get_max_size_bits(void)
	{
		return this->max_size_bits;
	}



};

Config_fstest *get_global_cfg(void);

#endif /* CONFIG_H_ */
