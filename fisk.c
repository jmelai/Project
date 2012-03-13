/*
Group Name: FastFisky
Members: Erik Afable, Nazma Panjwani, James Xiang & Jamie Lai

Assignment 3 Part 1
*/

#include "fisk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BLOCK_SIZE 1024

static int fd;

/* opens the "fisk" for reading/writing */
int openDisk(char *filename, int nbytes)
{	
	// opens the fisk; if it does not exist, a new one is created
	fd = open(filename, O_RDWR | O_CREAT, (mode_t)0600);
	if (fd == -1) {
		perror("Error opening file for writing");
        return -1;
    }
	
    // ensures the file is of size nbytes 
	lseek(fd, nbytes-1, SEEK_SET);
	write(fd, "\0", 1);
	
	// returns the file descriptor
	return fd;
}

/* closes the "fisk" */
int closeDisk()
{
	int result;
	result = close(fd);
	
	if (result == -1) {
		perror("Error closing file");
		return -1;
	}
	
	return 0;
}

/* reads the block specified by blocknr and places the contents 
into the buffer block */
int readBlock(int disk, int blocknr, void *block)
{
	int result;
	
	// sets the file pointer to position specified by blocknr
	result = lseek(disk, blocknr*BLOCK_SIZE, SEEK_SET);
	
	if(result == -1) {
		perror("Error calling lseek() to set starting position in reading");
	}
	
	// obtains the contents of the block and places it in the block buffer
	result = read(disk, block, BLOCK_SIZE);
	
	if(result == -1) {
		perror("Error reading from disk");
	}
	
	return 0;
}

/* writes the contents of a single block from the buffer *block into the fisk block disk specified by the blocknr */
int writeBlock(int disk, int blocknr, void *block)
{
	int result;
	
	// sets the file pointer to position specified by blocknr
	result = lseek(disk, blocknr*BLOCK_SIZE, SEEK_SET);
	
	if(result == -1){
		perror("Error calling lseek() to set starting position in writing");
	}
	
	// writes the contents of the block buffer into the "fisk" block	
	result = write(disk, block, BLOCK_SIZE);
	
	if(result == -1){
		perror("Error writing to disk");
	}
	
	return 0;
}

/* forces any outstanding writes to the "fisk" immediately */
void syncDisk()
{
	int result;
	
	// syncs the "fisk"
	result = fsync(fd);
       
	if(result == -1){
		perror("Error synchronizing to disk");
	}
}


/*int main(int argc, char *argv[])
{
  	int fisky = openDisk("test.txt", 1024*10);

  	// initializes buffers to read/write
  	char* buffer;
  	char* buffread;
  	int i;
  	
  	buffer = (char*)malloc(sizeof(char)*1024);
  	buffread = (char*)malloc(sizeof(char)*1024);
  	
  	// writes to block specified
  	for (i=0; i<5; i++) {
  		buffer[i] = 'a';	
  	}
	printf("Writing to Block 0\n");
  	writeBlock(fisky, 0, buffer);
  	
  	for (i=0; i<5; i++) {
  		buffer[i] = 'b';	
  	}
  	printf("Writing to Block 1\n");
  	writeBlock(fisky, 1, buffer);
  	
  	for (i=0; i<5; i++) {
  		buffer[i] = 'c';	
  	}
  	printf("Writing to Block 6\n");
  	writeBlock(fisky, 6, buffer);
  	
  	// reads from block specified
  	printf("Reading from Block 0\n");
  	readBlock(fisky, 0, buffread);
  	for (i=0; i<5; i++) {
  		printf("buffread[%d] = %c\n", i, buffread[i]);	
  	}
  	
  	printf("Reading from Block 1\n");
  	readBlock(fisky, 1, buffread);
  	for (i=0; i<5; i++) {
  		printf("buffread[%d] = %c\n", i, buffread[i]);	
  	}
  	
  	printf("Reading from Block 6\n");
  	readBlock(fisky, 6, buffread);
  	for (i=0; i<5; i++) {
  		printf("buffread[%d] = %c\n", i, buffread[i]);	
  	}	
  	
  	syncDisk();
  	closeDisk();
  	
	return 0;

}*/

/*
int main() {

	int fisky = openDisk("diskfile", 1024*1000);
	char* test_buffer = (char*)malloc(sizeof(int)*256);	
	int* buffer = (int*)malloc(sizeof(int)*256);
	
	int i, h;
	for (h=50; h < 55; ++h)
	{
		printf("\nBLOCK NUMBA IS %i\n\n", h);
		readBlock(fisky, h, test_buffer);
		for (i=0; i<1024; i++) {
	  		printf("test_buffer[%d] = %c\n", i, test_buffer[i]);	
	  	}
	}
	
	
	for (h=50; h < 55; ++h)
	{
		printf("\nBLOCK NUMBA IS %i\n\n", h);
		readBlock(fisky, h, buffer);
		for (i=0; i<256; i++) {
	  		printf("buffer[%d] = %i\n", i, buffer[i]);	
	  	}
	}

	return 0;
}
*/




