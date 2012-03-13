/*
Using the project to test various software configuration systems.

File: effs_test.c
Group Name: ExtremelyFastFiskSystem
Members: Erik Afable, Jamie Lai, Nazma Panjwani & James Xiang
Assignment 3 Part 2
*/

#include "effs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 10000
#define CHAR_BUFFER_SIZE 10000
#define INT_BUFFER_SIZE 2500

#define FISK_SIZE 1000

/* Simplifying Assumptions:
 *
 *	INITIALIZATION:
 *		We have only one fisk of a predetermined size FISK_SIZE.
 *		We cover two cases for a fisk initialization:
 *			1. Fisk does not exists and is created of size
 *				FISK_SIZE*1024 Bytes.
 *			2. Fisk already exists and is of size FISK_SIZE*1024
 *				Bytes.
 *
 *	DIRECTORIES
 *		Directories are a fixed size of 1 block (1024 Bytes).
 *		Directories can have at most 48 files contained within
 *		 them (48 sub-directories and "." and "..")
 */

/* Global Statics */
static int errors;

/* Function Prototypes */
int test_create(char* test, char* fname, int flag, int filesize);
int test_open(char* test, char* fname);
int test_remove(char* test, char* fname);




int test_create(char* test, char* fname, int flag, int filesize)
{
	if ( effs_create(fname, FILE, filesize) == 0 )
		fprintf(stdout, "%s Passed\n", test);
	else
	{
		fprintf(stderr, "%s **Failed**\n", test);
		++errors;
		return -1;
	}

	return 0;
}

int test_open(char* test, char* fname)
{
	int fd;

	fd = effs_open(fname);

	if ( fd != 0 )
		fprintf(stdout, "%s Passed\n", test);
	else
	{
		fprintf(stderr, "%s **Failed**\n", test);
		++errors;
		return -1;
	}
	return fd;
}

int test_remove(char* test, char* fname)
{
	fprintf(stdout, "%s removing file %s\n", test, fname);
	if ( effs_delete(fname) == -1 )
	{
		fprintf(stderr, "%s--\t **Failed**\n", test);
		++errors;
		return -1;
	} else {
		fprintf(stdout, "%s--\t Passed\n", test);
	}
	return 0;
}

int main(int argc, char* argv[])
{
	errors = 0;
	int size;
	int i;
	void* buffer;
	char* c_buffer;
	int* i_buffer;

	char* test;
	char* fname;
	int fildes;

	// Initialize test buffer used for testing
	buffer = (void*)malloc(BUFFER_SIZE);
	c_buffer = (char*)buffer;
	i_buffer = (int*)buffer;
	for (i = 0; i < CHAR_BUFFER_SIZE; ++i)
		c_buffer[i] = '\0';

	/****************************************************************/
	/** Test Part 1: test initialize disk of size FISK_SIZE blocks **/
	/****************************************************************/
	fprintf(stdout, "\n/* Test Part 1: test initialize disk of size %i Bytes */\n",
						FISK_SIZE*1024);
	// Test 1a: Test initialize disk of size 10000 blocks
	size = FISK_SIZE*1024;
	if ( effs_initialize(size) == 0 )
		fprintf(stdout, "Test 1a: Test initialize disk of size 10000 blocks--\t Passed\n");
	else
		fprintf(stderr, "Test 1a: Test initialize disk of size 10000 blocks--\t **Failed**\n");

	/****************************************************************/
	/** Test Part 2: test create files ******************************/
	/****************************************************************/
	fprintf(stdout, "\n/* Test Part 2: test create files */\n");

	// Test 2a: test create file of empty size
	size = 2000;
	test_create("Test 2a: test create file \"yvonne\" of empty size--\t", "yvonne", FILE, size);

	// Test 2b, 2c, 2d: test create 3 files of size 100
	size = 2000;
	test_create("Test 2b: test create a second file \"yvonne101\" of size 100--\t", "yvonne101", FILE, size);
	test_create("Test 2c: test create a third file \"yvonne102\" of size 100--\t", "yvonne102", FILE, size);
	test_create("Test 2d: test create a fourth file \"yvonne103\" of size 100--\t", "yvonne103", FILE, size);

	// Test 2e: test create a file that already exists
	fprintf(stdout, "Test 2e: test create a file that already exists \"yvonne101\"--\n\t");
	test = "Test 2e--\t";
	if ( effs_create("yvonne101", DIRECTORY, 0) == -1 )
		fprintf(stdout, "%s Passed\n", test);
	else
	{
		fprintf(stderr, "%s **Failed**\n", test);
		++errors;
		return -1;
	}

	/****************************************************************/
	/** Test Part 3: test create directories ************************/
	/****************************************************************/
	fprintf(stdout, "\n/* Test Part 3: test create directories */\n");

	// Test 3a: test create one directory
	test_create("Test 3a: test create one directory \"dir1\"--\t", "dir1", DIRECTORY, 0);

	// Test 3b, 3c, 3d: test create 3 directories
	test_create("Test 3b: test create a second directory \"dir2\"--\t", "dir2", DIRECTORY, 0);
	test_create("Test 3c: test create a second directory \"dir3\"--\t", "dir3", DIRECTORY, 0);
	test_create("Test 3d: test create a second directory \"dir4\"--\t", "dir4", DIRECTORY, 0);

	// Test 3e: test create a directory that already exists
	fprintf(stdout, "Test 3e: test create a directory that already exists \"dir1\"--\n\t");
	test = "Test 3e--\t";
	if ( effs_create("dir1", DIRECTORY, 0) == -1 )
		fprintf(stdout, "%s Passed\n", test);
	else
	{
		fprintf(stderr, "%s **Failed**\n", test);
		++errors;
		return -1;
	}

	/****************************************************************/
	/** Test Part 4: writing and reading to files *******************/
	/****************************************************************/
	fprintf(stdout, "\n/* Test Part 4: writing and reading to files */\n");

	// loading buffer with less than 1 block of data (10 Bytes)
	size = 10;
	for (i = 0; i < size; ++i)
		c_buffer[i] = 'a';

	// Test 4a: test opening a file
	test = "Test 4a: test opening file \"yvonne\"--\t";
	fprintf(stdout, "%s", test);
	test = "Test 4a:--\t";
	fildes = test_open(test, "yvonne");

	if (fildes > 0)
	{
		// Test 4b: test writing to file less than 1 block (10 Bytes)
		fprintf(stdout, "Test 4b: test writing to file desc. inode[%i] %i Bytes\n", fildes, size);
		test = "Test 4b:--\t";
		if ( effs_write(fildes, buffer, size) == 0 )
			fprintf(stdout, "%s Passed\n", test);
		else
		{
			fprintf(stderr, "%s **Failed**\n", test);
			++errors;
			return -1;
		}

		// Test 4c: test reading from file that was written to
		fprintf(stdout, "Test 4c: test reading from file desc. inode[%i] %i Bytes\n", fildes, size);
		test = "Test 4c:--\t";
		if ( effs_read("yvonne", buffer, size) == 0 )
		{
			for (i = 0; i < size; ++i)
				if ( c_buffer[i] != 'a' )
				{
					fprintf(stderr, "%s **Failed**\n", test);
					++errors;
					return -1;
				}
			fprintf(stdout, "%s Passed\n", test);
		}
		else
		{
			fprintf(stderr, "%s **Failed**\n", test);
			++errors;
			return -1;
		}
	}

	// loading buffer with greater than 1 block of data (2000 Bytes)
	size = 2000;
	fname = "yvonne102";

	for (i = 0; i < size; ++i)
		c_buffer[i] = 'b';

	// Test 4d: test opening a file
	test = "Test 4d:--\t";
	fprintf(stdout, "Test 4d: test opening file \"%s\"--\t", fname);
	fildes = test_open(test, fname);
	if (fildes > 0)
	{
		// Test 4e: test writing to file less than 1 block (2000 Bytes)
		fprintf(stdout, "Test 4e: test writing to file desc. inode[%i] %i Bytes\n", fildes, size);
		test = "Test 4e:--\t";
		if ( effs_write(fildes, buffer, size) == 0 )
			fprintf(stdout, "%s Passed\n", test);
		else
		{
			fprintf(stderr, "%s **Failed**\n", test);
			++errors;
		}

		// Test 4f: test reading from file that was written to
		fprintf(stdout, "Test 4f: test reading from file desc. inode[%i] %i Bytes\n", fildes, size);
		test = "Test 4f:--\t";
		if ( effs_read(fname, buffer, size) == 0 )
		{
			for (i = 0; i < size; ++i)
				if ( c_buffer[i] != 'b' )
				{
					fprintf(stderr, "%s **Failed**\n", test);
					++errors;
				}
			fprintf(stdout, "%s Passed\n", test);
		}
		else
		{
			fprintf(stderr, "%s **Failed**\n", test);
			++errors;
		}
	}

	/****************************************************************/
	/** Test Part 5: removing files and directories *****************/
	/****************************************************************/

	fprintf(stdout, "\n/* Test Part 5: removing files and directories */\n");

	// Test 5a: removing all yvonne files
	test = "Test 5a:";
	test_remove(test, "yvonne");
	test_remove(test, "yvonne101");
	test_remove(test, "yvonne102");
	test_remove(test, "yvonne103");

	// Test 5b: removing all directories dir
	test = "Test 5b:";
	test_remove(test, "dir1");
	test_remove(test, "dir2");
	test_remove(test, "dir3");
	test_remove(test, "dir4");

	/****************************************************************/
	/** Test Part 6: testing sync ***********************************/
	/****************************************************************/



	fprintf(stdout, "\nTest harness found %i errors.\n", errors);

	return 0;
}



