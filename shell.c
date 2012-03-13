/*
File: shell.c
Group Name: FastFisky
Members: Erik Afable, Jamie Lai, Nazma Panjwani & James Xiang
Assignment 3 Part 2
*/

#include "fisk.h"
#include "effs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>

#define FISK_SIZE 1000
#define BUFFER_SIZE 20000

static int fp = -1;

static int parsecmd(char msg[]);
static int test_fisk(int startBlock, int endBlock, int type);

static void* read_buffer;
static char* c_read_buffer;
static int* i_read_buffer;
static int char_read_length;
static int int_read_length;

int parsecmd(char msg[]) {
	char delims[] = " ";
	char* result = NULL;
	char command[10][100];
	char* reset_filesize = "0";
	int count = 0;
	int i, j;
	int n;

	char* content = (char*)malloc(sizeof(char)*BUFFER_SIZE);
	for (i = 0; i < BUFFER_SIZE; i++) {
  		content[i] = 'a';	
  	}
	
	// 1 = create_dir, 2 = create_file, 3 = change_dir, 4 = print_curr_dir, 6 = delete_file, 
	// 7 = open_file, 8 = write, 9 = test, 10 = read
	int flag; 
	
	result = strtok(msg, delims);
	if (strcmp(result, "mkdir") == 0) {
		flag = 1; 
		result = strtok(NULL, delims);
	} else if (strcmp(result, "mkfile") == 0) {
		flag = 2;
		result = strtok(NULL, delims);
	} else if (strcmp(result, "cd") == 0) {
		flag = 3;
		result = strtok(NULL, delims);
	} else if (strcmp(result, "ls") == 0) {
		flag = 4;
	} else if (strcmp(result, "pwd") == 0) {
		flag = 5;
	} else if (strcmp(result, "rm") == 0) {
		flag = 6;
		result = strtok(NULL, delims);
	} else if (strcmp(result, "open") == 0) {
		flag = 7;
		result = strtok(NULL, delims);
	} else if (strcmp(result, "write") == 0) {
		flag = 8;
		result = strtok(NULL, delims);
	} else if (strcmp(result, "test") == 0) {
		flag = 9;
		result = strtok(NULL, delims);
	} else if (strcmp(result, "read") == 0) {

		flag = 10;
		result = strtok(NULL, delims);
	}
	
	while (result != NULL) {
		strcpy(command[count++], result);
		result = strtok(NULL, delims);
	}
	
	if (flag == 1) {
		if (count == 0) {
			printf("Must enter name for directory!\n");
			return -1;
		} else if (strlen(command[0]) > 9) {
			printf("Must enter at most 9 characters for name!\n");
			return -1;
		}
		create_dir(command[0]);
	} else if (flag == 2) {
		if (count == 0) {
			printf("Must enter name for file!\n");
			return -1;
		} else if (strlen(command[0]) > 9) {
			printf("Must enter at most 9 characters for name!\n");
			return -1;
		}
		create_file(atoi(command[1]), command[0]);
		for (i = 0; i < strlen(reset_filesize); ++i)
			command[1][i] = reset_filesize[i];
		for (i = strlen(reset_filesize); i < 100; ++i)
			command[1][i] = '\0';
	} else if (flag == 3) { 
		if (count == 0) {
			printf("Must enter name for directory!\n");
			return -1;
		}
		change_dir(command[0]);
	}else if (flag == 4) {
		print_curr_dir();
	} else if (flag == 5) {
		get_curr_dir();
	} else if (flag == 6) {
		if (count == 0) {
			printf("Must enter name for file!\n");
			return -1;
		}
		effs_delete(command[0]);
		fp = -1;
	} else if (flag == 7) {
		if (count == 0) {
			printf("Must enter name for file!\n");
			return -1;
		}
		fp = effs_open(command[0]);
	} else if (flag == 8) {
		if (fp == -1) {
			printf("A file needs to be opened first!\n");
			return -1;
		}
		if (count == 0) {
			printf("Must enter content size!\n");
			return -1;
		}
		/* effs$ write <filename> <content> <filesize> */
		effs_write(fp, content, atoi(command[0])); 
	} else if (flag == 9) {
		test_fisk(atoi(command[0]), atoi(command[1]), atoi(command[2]));
	} else if (flag == 10) {
		/* effs$ read <filename> <filesize> <mode> */
		char_read_length = atoi(command[1]);
		int_read_length = atoi(command[1])/4;

		for (i = 0; i < 10000; ++i)
			c_read_buffer[i] = '\0';

		n = effs_read(command[0], read_buffer, atoi(command[1]));
		if (n == -1)
			return -1;

		/* setting modes
		 *	char mode = 1, int mode = 2 */
		if ( atoi(command[2]) == 1 )
			for (i = 0; i < char_read_length; ++i)
				printf("%i = %c\n", i, c_read_buffer[i]);
		else if ( atoi(command[2]) == 2 )
			for (i = 0; i < int_read_length; ++i)
				printf("%i = %i\n", i, i_read_buffer[i]);
	}

	// flush command buffer
	for (i = 0; i < 10; ++i)
		for (j = 0; j < 100; ++j)
			command[i][j] = '\0';
	
	return 0;
}

int test_fisk(int startBlock, int endBlock, int type) {
	int fisky = openDisk("diskfile", 1024*1000);
	char* char_buffer = (char*)malloc(sizeof(char)*1024);	
	int* int_buffer = (int*)malloc(sizeof(int)*256);
	int i, j;
	
	/* 1 for char, 2 for int */
	if (type == 1) {
		for (i = startBlock; i < endBlock + 1; i++) {
			printf("\nBLOCK NUMBER IS %i\n\n", i);
			readBlock(fisky, i, char_buffer);
			for (j=0; j<1024; j++) {
	  			printf("char_buffer[%d] = %c\n", j, char_buffer[j]);	
	  		}
		}
	} else if (type == 2) {
		for (i = startBlock; i < endBlock + 1; i++) {
			printf("\nBLOCK NUMBER IS %i\n\n", i);
			readBlock(fisky, i, int_buffer);
			for (j=0; j<256; j++) {
	  			printf("int_buffer[%d] = %i\n", j, int_buffer[j]);	
	  		}
		}
	}
	return 0;
}


int main(int argc, char *argv[], char *envp[]) {
	effs_initialize(FISK_SIZE); // initializes diskfile

	read_buffer = (void*)malloc(10000);
	c_read_buffer = (char*)read_buffer;
	i_read_buffer = (int*)read_buffer;

	char msg[512], c;
	int n = 0, i = 0;
	
	printf("effs$ ");
	while ((c = getchar()) != EOF) {
		if (n == 0 && c == '\n') {
			printf("effs$ ");
			continue;	
		}
		
		msg[n++] = c;
		
		while ((c = getchar()) != '\n') {
			msg[n++] = c;
		}
		msg[n] = '\0';
		if (strcmp(msg, "exit") == 0 || strcmp(msg, "quit") == 0) {
			effs_close(0);
			effs_sync(0);
			printf("\n** END **\n\n");
			break;
		} else {
			// passes user input to be parsed
			parsecmd(msg);
			// clears user input
			for (i=0; i<512; i++)
			{
				msg[i] = '\0';
			}
			printf("effs$ ");
			n = 0;
		}
	}
	printf("\n");
	return 0;
}


