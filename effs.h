/*
File: effs.h
Group Name: FastFisky
Members: Erik Afable, Jamie Lai, Nazma Panjwani & James Xiang
Assignment 3 Part 2
*/

#include "fisk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* defines */
#define OCCUPIED 1
#define FREE 0
#define DIRECTORY 1
#define FILE 0
#define EFFS_ROOT_CHECKSUM 1337
#define INODE_SIZE 18
#define MAX_FILECOUNT 48

// interface functions
int effs_initialize(int numblocks);
int effs_create(char* name, int flag, int filesize);
int effs_delete(char* filename);
int effs_open(char* filename);
int effs_write(int inode_index, void *file_content, int filesize);
int effs_read(char* filename, void *file_content, int filesize);
int effs_sync(int fildes);
int effs_close(int fildes);

// additional functions for shell
int create_file(int filesize, char* filename);
int create_dir(char* dirname);
void print_curr_dir();
void change_dir(char* dirname);
int get_curr_dir();


