/*
Group Name: FastFisky
Members: Erik Afable, Jamie Lai, Nazma Panjwani & James Xiang

Assignment 3 Part 2
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BLOCK_SIZE 1024

int openDisk(char *filename, int nbytes);

int closeDisk();

int readBlock(int disk, int blocknr, void *block);

int writeBlock(int disk, int blocknr, void *block);

void syncDisk();
