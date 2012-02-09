/*
 * Configuration options for the fstest utility
 *
 *    Copyright (C) 2011 Fraunhofer ITWM, Bernd Schubert
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

class Config_fstest {
public:
	Config_fstest(void)
	{
		this->usage_percent = 90;
		this->immediate_check = false;
		this->testdir = "";
	}

private:
	double usage_percent; // max fill level
	bool immediate_check;
	string testdir;

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
};

Config_fstest *get_global_cfg(void);

#endif /* CONFIG_H_ */
