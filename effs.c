/*
This is a comment.
File: effs.c
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
#define BLOCK_SIZE 1024
#define BUFFER_SIZE 1024
#define EFFS_ROOT_CHECKSUM 1337
#define INODE_SIZE 18
#define MAX_FILECOUNT 48
#define FILE_NAME_SIZE 10

//#define DEBUG // for testing purposes

/* structs */
typedef struct superblock {
	int* block_freelist; // list of free data blocks + DATA_OFFSET
	int* inode_freelist;
	int free_data_space;
	int rootinode;
} *superblock_t;

typedef struct directory {
	int filecount;
	int* inode_num;
	char** filename;
} *directory_t;

typedef struct inode {
	int filecode; 	// 0 for dir, 1 for file
	int filesize;
	int inode_num;
	int* diskmap;
} *inode_t;

/* global statics */
static int fisky;
static int super_block_size; // number of blocks super block occupies
static int num_of_inodes; // number of inodes
static int data_block_size;  // number of data blocks
static int DATA_OFFSET;
static int INODE_OFFSET;
static int* inode_buffer;
static int* p_inode_buffer;
static void* data_buffer;
static inode_t node;
static superblock_t super;
static directory_t curr_dir;
static directory_t new_dir;
static char* empty_file;
static int* single_array;
static int* double_array;
static int* triple_array;
static int* null_array;

/* function prototypes */
static void initialize_super_blocks();
static void initialize_inode_blocks();
static void initialize_root_dir();
static void read_super_blocks();
static void write_super_blocks();
static int get_free_inode();
static void read_inode(int inode_index);
static int remove_inode(int inode_index);
static int add_inode(int inode_index, int filecode, int filesize);
static int get_free_data_block();
static void reset_indirection(int* array);
static int print_data_buffer(char* p_data_buffer, int amount);

/* interface functions */
int effs_initialize(int numblocks);
int effs_create(char* name, int flag, int filesize);
int effs_delete(char* filename);
int effs_open(char* filename);
int effs_write(int inode_index, void *file_content, int filesize);
int effs_read(char* filename, void *file_content, int filesize);
int effs_sync(int fildes);
int effs_close(int fildes);

/* additional functions for shell */
int create_file(int filesize, char* filename);
int create_dir(char* dirname);
void print_curr_dir();
void change_dir(char* dirname);
int get_curr_dir();

/* @desc: prints the contents of current directory.
 */
void print_curr_dir() {
	int i;

	// search for curr dir. starting i @ 2 because of . and ..
	for (i = 2; i < curr_dir->filecount + 2; i++) {
		read_inode(curr_dir->inode_num[i]);
		if (node->filecode == FILE)
			printf("\tFILE----------");
		else
			printf("\tDIRECTORY-----");
		printf("%s\n", curr_dir->filename[i]);
	}
}

/* @desc: initialize function called by shell.
 * @rets: 0 if successful, -1 otherwise.
 */
int effs_initialize(int numblocks){
	int i, j;
	
	fisky = openDisk("diskfile", BLOCK_SIZE*numblocks);

	if (fisky == -1)
		return -1;

	int* super_buffer;
	super_buffer = (int*)malloc(sizeof(int)*BUFFER_SIZE);
	data_buffer = (void*)malloc(BUFFER_SIZE);
	empty_file = (char*)malloc(FILE_NAME_SIZE);
	
	empty_file[0] = '$';
	empty_file[1] = 'e';
	empty_file[2] = 'f';
	empty_file[3] = 'f';
	empty_file[4] = 's';
	empty_file[5] = '$';

	for (i = 6; i < 10; ++i)
		empty_file[i] = '\0';

	/****** CALCULATE FISK OFFSETS *******/
	/****** CALCULATE FISK OFFSETS *******/

	/* calc. number of total inodes, data blocks, and num of super blocks */
	num_of_inodes = (int)(0.05 * numblocks * 10) + 1;
	data_block_size = (int)(0.95 * numblocks) + 1;
	super_block_size =
		((
		  sizeof(int)*(data_block_size) // block free list size
		+ sizeof(int)*(num_of_inodes) // inode free list size
		+ sizeof(int) * 2 // free_data_space & rootinode ints
		) / BLOCK_SIZE) + 1; // we're conservative! :)

	/* calc. DATA OFFSET and INODE OFFSET to start of datablocks */
	DATA_OFFSET = (int)(0.05 * numblocks);
	INODE_OFFSET = super_block_size;
	
#ifdef DEBUG
	printf("INODE_OFFSET = %i\n", INODE_OFFSET);
	printf("DATA_OFFSET + %i\n", DATA_OFFSET);
#endif

	/* init super block struct */
	super = (superblock_t)malloc(sizeof(struct superblock));
	super->block_freelist = (int*)malloc (sizeof(int) * data_block_size); 
	super->inode_freelist = (int*)malloc(sizeof(int)*(num_of_inodes));
	super->free_data_space = BLOCK_SIZE*data_block_size;
	super->rootinode = 0; // actual inode number for root dir
	
	/* init inode struct */
	node = (inode_t)malloc(sizeof(struct inode));
	node->filecode = DIRECTORY;
	node->filesize = BLOCK_SIZE;
	node->inode_num = 0;
	node->diskmap = (int*)malloc( sizeof(int)*15 );
	
	inode_buffer = (int*)malloc(sizeof(int)*256);
	p_inode_buffer = &inode_buffer[0];
	
	/* pointing current directory to root */
	curr_dir = (directory_t)malloc(sizeof(struct directory));
	curr_dir->inode_num = (int*)malloc(sizeof(int)*50);
	
	/* initialize 2d array of filenames and null the 50 10-byte char strings */
	curr_dir->filename = (char**)malloc(sizeof(char*)*50);

	for (i = 0; i < 50; ++i)
	{
		curr_dir->filename[i] = (char*)malloc(sizeof(char)*FILE_NAME_SIZE);
		for (j = 0; j < FILE_NAME_SIZE; ++j)
			curr_dir->filename[i][j] = '\0';
	}
	
	/* dummy directory for creating new directories */
	new_dir = (directory_t)malloc(sizeof(struct directory));
	new_dir->inode_num = (int*)malloc(sizeof(int)*50);
	
	new_dir->filename = (char**)malloc(sizeof(char*)*50);
	for (i = 0; i < 50; ++i)
	{
		new_dir->filename[i] = (char*)malloc(sizeof(char)*FILE_NAME_SIZE);
		for (j = 0; j < FILE_NAME_SIZE; ++j)
			new_dir->filename[i][j] = '\0';
	}
	
	// malloc'ing space for single, double & triple indirection arrays
	single_array = (int*)malloc(sizeof(int)*256);
	double_array = (int*)malloc(sizeof(int)*256);
	triple_array = (int*)malloc(sizeof(int)*256);
	
	// null array for nulling out blocks
	null_array = (int*)malloc(sizeof(int)*256);
	for (i = 0; i < 256; i++)
		null_array[i] = 0;
	
	readBlock(fisky, 0, super_buffer); // test first block for EFFS standard
	
	if (  super_buffer[0] != EFFS_ROOT_CHECKSUM ) {
		// initialize information for a new fisk
		initialize_super_blocks();
		initialize_inode_blocks();
		initialize_root_dir();
	} else {
		read_super_blocks();
		change_dir("root");
	}
	return 0;
}

/* @desc: initializes superblocks and writes its information to fisk.
 */
static void initialize_super_blocks() {
	int* super_buffer;
	int i;
	super_buffer = (int*)malloc(sizeof(int)*BUFFER_SIZE);
	
	/* marking all data blocks as free */
	super->block_freelist[0] = EFFS_ROOT_CHECKSUM; // because r00t iZ l33t

	for (i=1; i < data_block_size; i++)
		super->block_freelist[i] = FREE;
	
	/* marking first inode as root and rest as free */
	super->inode_freelist[0] = OCCUPIED;
	for (i=1; i < num_of_inodes; i++)
		super->inode_freelist[i] = FREE;
	
	/***** WRITING SUPER BLOCK TO FISK *****/
	/***** WRITING SUPER BLOCK TO FISK *****/
	
	int blocknr = 0; // keeps track of fisk block to write to
	int count = 0; // keeps track of buffer position

	/* loads one full block (256 ints per 1 block) to buffer and writes to fisk */
	for (i=0; i < data_block_size; i++)
	{
		super_buffer[count++] = super->block_freelist[i];		
		if ( (count % 256 == 0) && (count != 0) ) {
			writeBlock(fisky, blocknr, super_buffer);		
			blocknr++;
			count = 0;
		}
	}
	
	for (i=0; i < num_of_inodes; i++) {
		super_buffer[count++] = super->inode_freelist[i];
		if ( (count % 256 == 0) && (count != 0) ) {
			writeBlock(fisky, blocknr, super_buffer);		
			blocknr++;
			count = 0;
		}
	}
	
	if (count <= 254) {
		super_buffer[count++] = super->free_data_space;
		super_buffer[count++] = super->rootinode;

		/* clearing the rest of buffer for SANITY's sake */
		for (i = count; i < 256; ++i)
			super_buffer[i] = 0;

		writeBlock(fisky, blocknr, super_buffer);
	} else if (count == 255) {
		super_buffer[count++] = super->free_data_space;
		writeBlock(fisky, blocknr, super_buffer);
		count = 0;
		blocknr++;
		super_buffer[count++] = super->rootinode;

		/* clearing the rest of buffer for SANITY's sake */
		for (i = count; i < 256; ++i)
			super_buffer[i] = 0;

		writeBlock(fisky, blocknr, super_buffer);
	}
}

/* @desc: initializes all inode blocks, setting all to default except root.
 * @rets: 0 if successful, -1 otherwise.
 */
static void initialize_inode_blocks() {
	int i, j, k;
	
	/* init inode buffer values to 0 */
	for (i = 0; i < 256; ++i)
		inode_buffer[i] = 0;
	
	/***** INITIALIZE INODE SPACE *****/
	/***** INITIALIZE INODE SPACE *****/
	int inode_id = 0;
	int inode_count = 0;
	int inode_blocknr = 0;

	/* initialize root's diskmap pointers to 0 */
	node->diskmap[0] = DATA_OFFSET;
	for (i = 1; i < 15; ++i)
		node->diskmap[i] = 0;

	/* init inode */
	for (i = 0; i < num_of_inodes; ++i) {
		if (i != 0) {
			node->diskmap[0] = 0;
		}
		
		/* traverse inode_buffer and input new inode info (18 Bytes) */
		node->inode_num = inode_id++;
		*p_inode_buffer++ = node->filecode;
		*p_inode_buffer++ = node->filesize;
		*p_inode_buffer++ = node->inode_num;
		
		for (j = 0; j < 15; ++j)
			*p_inode_buffer++ = node->diskmap[j];	
			
		++inode_count;

		if ( inode_count == 14 ) {
			writeBlock(fisky, inode_blocknr + INODE_OFFSET, inode_buffer);
			/* clear inode buffer after write */
			
			for (k = 0; k < 256; ++k)
				inode_buffer[k] = 0;
			
			/* reset p_inode_buffer to beginning */
			p_inode_buffer = &inode_buffer[0];
			++inode_blocknr;
			inode_count = 0;
		}
	}
	
	/* write last inode_block to fisk */
	writeBlock(fisky, inode_blocknr + INODE_OFFSET, inode_buffer);
}

/* @desc: initializes the root directory writing out to its data blocks.
 * @rets: 0 if successful, -1 otherwise.
 */
static void initialize_root_dir() {	
	int i, j, k;
	int* int_data_buffer = (int*)data_buffer;
	char* char_data_buffer = (char*)data_buffer;

	/* temp_filename is a placeholder for filename passed in, first it is root */
	char* temp_filename = ".";
	curr_dir->filecount = 0;
	curr_dir->inode_num[0] = 0;

	/* write string temp_filename to current directory's filename (this is for root) */
	for (i = 0; i < strlen(temp_filename); ++i)
		curr_dir->filename[0][i] = temp_filename[i];
	
	/* place holder for ".." since root has no parent so that our directory table is consistent */
	temp_filename = "..";
	curr_dir->inode_num[1] = -3;
	/* write string temp_filename to current directory's filename (this is for root) */
	for (i = 0; i < strlen(temp_filename); ++i)
		curr_dir->filename[1][i] = temp_filename[i];

	/* setting inode numbers after root as -1 (since not in use yet) */
	for (i = 2; i < 50; i++) {
		curr_dir->inode_num[i] = -1;
	}

	temp_filename = "$effs$"; // so that we can keep track of empty spots in directory

	/* loads strings after root to our temp_filename and then into current dir's filename[index] */
	for (i = 2; i < 50; i++)
		for (j = 0; j < strlen(temp_filename); ++j)
			curr_dir->filename[i][j] = temp_filename[j];

	/* loading filecount as first thing in data buffer (0-byte to 3-byte) */
	int_data_buffer[0] = curr_dir->filecount;
	
	/* loading inode numbers as next 1 to 51 things in data buffer (4-byte to 203-byte) */
	for (i = 1; i < 51; i++) {
		int_data_buffer[i] = curr_dir->inode_num[i-1];
	}
	  
	/* putting the 9-char strings (10 including null) to data buffer. 50 of these (204-byte to 703-byte) */
	k = 0;
	for (i = 204; i < 254; i++, k++)
		for (j = 0; j < FILE_NAME_SIZE; j++)
			char_data_buffer[i + j + k*9] = curr_dir->filename[i-204][j];

	/* nulling out the rest of the data buffer that is unused (704-byte to 1023-byte) */
	for (i = 704; i < BUFFER_SIZE; i++)
		char_data_buffer[i] = '\0';
	
	writeBlock(fisky, DATA_OFFSET, data_buffer);
}

/* @desc: reads current superblock information from fisk into superblock struct.
 * @rets: 0 if successful, -1 otherwise.
 */
static void read_super_blocks() {
	int* super_buffer;
	int i;
	super_buffer = (int*)malloc(sizeof(int)*BUFFER_SIZE);

	int blocknr = 0;
	int count = 0;
	readBlock(fisky, blocknr, super_buffer);
	
	for (i = 0; i < data_block_size; i++) {		
		super->block_freelist[i] = super_buffer[count++];
		if ( (count % 256 == 0) && (count != 0) ) {	
			blocknr++;
			count = 0;
			readBlock(fisky, blocknr, super_buffer);
		}
	}
	
	for (i = 0; i < num_of_inodes; i++) {		
		super->inode_freelist[i] = super_buffer[count++];
		if ( (count % 256 == 0) && (count != 0) ) {
			blocknr++;
			count = 0;
			readBlock(fisky, blocknr, super_buffer);
		}
	}
	
	if (count <= 254) {
		super->free_data_space = super_buffer[count++];
		super->rootinode = super_buffer[count];
	} else if (count == 255) {
		super->free_data_space = super_buffer[count];
		count = 0;
		blocknr++;		
		readBlock(fisky, blocknr, super_buffer);
		super->rootinode = super_buffer[count];
	}
}

/* @desc: writes all super block information to fisk.
 * @rets: 0 if successful, -1 otherwise.
 */
static void write_super_blocks() {
	int* super_buffer;
	int i;
	super_buffer = (int*)malloc(sizeof(int)*BUFFER_SIZE);
	
	/***** WRITING SUPER BLOCK TO FISK *****/
	/***** WRITING SUPER BLOCK TO FISK *****/
	
	int blocknr = 0; // keeps track of fisk block to write to
	int count = 0; // keeps track of buffer position

	/* loads one full block (256 ints per 1 block) to buffer and writes to fisk */
	for (i=0; i < data_block_size; i++)
	{
		super_buffer[count++] = super->block_freelist[i];		
		if ( (count % 256 == 0) && (count != 0) ) {
			writeBlock(fisky, blocknr, super_buffer);		
			blocknr++;
			count = 0;
		}
	}
	
	for (i=0; i < num_of_inodes; i++) {
		super_buffer[count++] = super->inode_freelist[i];
		if ( (count % 256 == 0) && (count != 0) ) {
			writeBlock(fisky, blocknr, super_buffer);		
			blocknr++;
			count = 0;
		}
	}
	
	if (count <= 254) {
		super_buffer[count++] = super->free_data_space;
		super_buffer[count++] = super->rootinode;

		/* clearing the rest of buffer for SANITY's sake */
		for (i = count; i < 256; ++i)
			super_buffer[i] = 0;

		writeBlock(fisky, blocknr, super_buffer);
	} else if (count == 255) {
		super_buffer[count++] = super->free_data_space;
		writeBlock(fisky, blocknr, super_buffer);
		count = 0;
		blocknr++;
		super_buffer[count++] = super->rootinode;

		/* clearing the rest of buffer for SANITY's sake */
		for (i = count; i < 256; ++i)
			super_buffer[i] = 0;

		writeBlock(fisky, blocknr, super_buffer);
	}
}

/* @desc: grabs the next free inode from the inode freelist, setting
 *	it to occupied.
 * @rets: 0 if successful, -1 otherwise.
 */
static int get_free_inode() {
	int i, result;
	int inode_index;
	
	result = 0;
	for (i = 0; i < num_of_inodes; i++) {		
		if (super->inode_freelist[i] == 0) {
			inode_index = i;
			super->inode_freelist[i] = OCCUPIED;
			result = 1;
			break;
		}
	}
	
	if (result == 1) {
		write_super_blocks();
		return inode_index;
	} else
		return -1;
	
}

/* @desc: reads inode specified by inode_index into the node struct.
 */
static void read_inode(int inode_index) {
	int i;
	int inode_block_num;
	int inode_start_index;
	
	/* calculates the block number the inode resides in 
		and its index within that block */
	inode_block_num = (inode_index / 14) + INODE_OFFSET;
	inode_start_index = (inode_index % 14);
	
	/* reads the block where the inode resides into the inode buffer
		and sets the pointer to the inode's index in the buffer */
	readBlock(fisky, inode_block_num, inode_buffer);
	
	p_inode_buffer = &inode_buffer[inode_start_index*INODE_SIZE];
	
	/* loads the inode from the inode buffer into a inode struct */
	node->filecode = *p_inode_buffer++;
	node->filesize = *p_inode_buffer++;
	node->inode_num = *p_inode_buffer++;
		
	for (i = 0; i < 15; ++i) {
		node->diskmap[i] = *p_inode_buffer++;
	}
}

/* @desc: adds a free new inode for a newly created file and reserves
 *	data blocks based on filesize and sets them to the new inode's
 *	diskmap.
 * @rets: 0 if successful, -1 otherwise.
 */
static int add_inode(int inode_index, int filecode, int filesize) {
	int i, j, k, m;
	int inode_block_num;
	int inode_start_index;
	int num_blocks_needed, remaining_blocks;
	int block_index;
	
	/*** UPDATING INODE WHEN CREATING A DIRECTORY ***/
	/*** UPDATING INODE WHEN CREATING A DIRECTORY ***/
	if (filecode == DIRECTORY) {
		/* grabs a free data block index from the block free list */
		block_index = get_free_data_block();
		if (block_index == -1) 
			return -1;
		
		/* set new directory's diskmap pointers to the free block's index */
		node->diskmap[0] = block_index;

		/* calculates the block number the inode resides in 
		and its index within that block */
		inode_block_num = (inode_index / 14) + INODE_OFFSET;
		inode_start_index = (inode_index % 14);
	
		/* reset p_inode_buffer to beginning of inode index */
		p_inode_buffer = &inode_buffer[inode_start_index*INODE_SIZE];
			
		/* loads the struct onto the inode buffer */
		*p_inode_buffer++ = node->filecode;
		*p_inode_buffer++ = node->filesize;
		*p_inode_buffer++ = node->inode_num;
		*p_inode_buffer++ = node->diskmap[0];

		writeBlock(fisky, inode_block_num, inode_buffer);
		return 0;
	}
	
	/*** UPDATING INODE WHEN CREATING A FILE ***/
	/*** UPDATING INODE WHEN CREATING A FILE ***/
	
	/* calculates the block number the inode resides in 
		and its index within that block */
	inode_block_num = (inode_index / 14) + INODE_OFFSET;
	inode_start_index = (inode_index % 14);

	/* calculates the number of data blocks needed to store file content*/
	num_blocks_needed = filesize / BLOCK_SIZE;
	if ( (filesize % BLOCK_SIZE) != 0)
		num_blocks_needed++;
	
	/** calculating number of blocks needed for indirections **/
	/** calculating number of blocks needed for indirections **/
	
	// calculating the number of pointer blocks needed for indirections
	int pointer_blocks = 0;
	int rem = num_blocks_needed;
	int temp;

	rem = rem - 12;
	// single indirection
	if (rem > 0)
		pointer_blocks++;
	rem = rem - 256;
	// double indirection
	if (rem > 0) {
		pointer_blocks++;
		for (i = 0; i < 256; i++) {
			pointer_blocks++;
			rem = rem - 256;
			if (rem <= 0)
				break;
		}
	}
	// triple indirection
	if (rem > 0) {
		pointer_blocks++;
		for (i = 0; i < 256; i++) {
			pointer_blocks++;
			for (j = 0; j < 256; j++) {
				pointer_blocks++;
				rem = rem - 256;
				if (rem <= 0)
					break;
			}
			if (rem <= 0)
				break;
		}
	}
	
	// checking if actual number of blocks needed is greater than free data space
	temp = pointer_blocks * BLOCK_SIZE;
	temp = temp + filesize;
	if (temp > super->free_data_space) {
		printf("File is too big!\n");
		return -1;
	}
	
	/** getting free data blocks for indirections **/
	/** getting free data blocks for indirections **/
	
	remaining_blocks = num_blocks_needed;
	
#ifdef DEBUG
	printf("num_blocks_needed = %i\n", num_blocks_needed);
#endif
	
	for (i = 0; i < 12; i++) {
		node->diskmap[i] = get_free_data_block();
		writeBlock(fisky, node->diskmap[i], null_array);
		remaining_blocks--;
		if (remaining_blocks == 0)
			break;
	}

	if (num_blocks_needed > 12) {
		for (i = 0; i < 256; i++) {
			single_array[i] = get_free_data_block();
			writeBlock(fisky, single_array[i], null_array);
			remaining_blocks--;
			if (remaining_blocks == 0)
				break;
		}
		if (remaining_blocks == 0) {
			for (j = i+1; j < 256; j++) {
				single_array[j] = -1;	
			}
		}
		
		block_index = get_free_data_block();
		node->diskmap[12] = block_index;
		writeBlock(fisky, block_index, single_array);
		reset_indirection(single_array);
	}
	
	if (num_blocks_needed > (12 + 256)) {
		for (k = 0; k < 256; k++) {	
			for (i = 0; i < 256; i++) {
				double_array[i] = get_free_data_block();
				writeBlock(fisky, double_array[i], null_array);
				remaining_blocks--;
				if (remaining_blocks == 0)
					break;
			}
			if (remaining_blocks == 0) {
				for (j = i+1; j < 256; j++) {
					double_array[j] = -1;	
				}
			}
			
			block_index = get_free_data_block();
			single_array[k] = block_index;
			writeBlock(fisky, block_index, double_array);
			reset_indirection(double_array);
			
			if (remaining_blocks == 0)
				break;
		}
		for (j = k+1; j < 256; j++) {
			single_array[j] = -1;	
		}
		
		block_index = get_free_data_block();
		node->diskmap[13] = block_index;
		writeBlock(fisky, block_index, single_array);
		reset_indirection(single_array);
	}
	
	if (num_blocks_needed > (12 + 256 + 65536)) {
		for (m = 0; m < 256; m++) {
			for (k = 0; k < 256; k++) {	
				for (i = 0; i < 256; i++) {
					triple_array[i] = get_free_data_block();
					writeBlock(fisky, triple_array[i], null_array);
					remaining_blocks--;
					if (remaining_blocks == 0)
						break;
				}
				if (remaining_blocks == 0) {
					for (j = i+1; j < 256; j++) {
						triple_array[j] = -1;	
					}
				}
				
				block_index = get_free_data_block();
				double_array[k] = block_index;
				writeBlock(fisky, block_index, triple_array);
				reset_indirection(triple_array);
				
				if (remaining_blocks == 0)
					break;
			}
			for (j = k+1; j < 256; j++) {
				double_array[j] = -1;	
			}
			
			block_index = get_free_data_block();
			single_array[m] = block_index;
			writeBlock(fisky, block_index, double_array);
			reset_indirection(double_array);	
		}
		block_index = get_free_data_block();
		node->diskmap[14] = block_index;
		writeBlock(fisky, block_index, single_array);
		reset_indirection(single_array);
	}

	/* reset p_inode_buffer to beginning of inode index */
	p_inode_buffer = &inode_buffer[inode_start_index*INODE_SIZE];
	
	/* update the inode attributes */
	node->filecode = filecode;
	node->filesize = filesize; 
	node->inode_num = inode_index;
	
	/* loads the struct onto the inode buffer */
	*p_inode_buffer++ = node->filecode;
	*p_inode_buffer++ = node->filesize;
	*p_inode_buffer++ = node->inode_num;
	
	/* loads the diskmap content onto the buffer to be written to the fisk */
	for (i = 0; i < 15; ++i)
		*p_inode_buffer++ = node->diskmap[i];
	
	/* writes the inode buffer onto fisk */
	writeBlock(fisky, inode_block_num, inode_buffer);
	
	/* updates the total free data space */
	super->free_data_space = super->free_data_space - filesize;
	write_super_blocks();
	
	return 0;
}

/* @desc: zeroes out an inode's diskmap and sets the inode specified by
 *	inode_index to unused default.
 * @rets: 0 if successful, -1 otherwise.
 */
static int remove_inode(int inode_index) {
	
	int i, j, k;
	int inode_block_num;
	int inode_start_index;
	int indirection_data_blocks = 0;
	
	read_inode(inode_index);
#ifdef DEBUG
	printf("filesize is %i\n", node->filesize);
#endif
	/** CLEARING INODE DISKMAP OF node **/
	/** CLEARING INODE DISKMAP OF node **/
	
	/* clear first 12 data blocks of current node */
	for (i = 0; i < 12; ++i) {
		
		if (node->diskmap[i] != 0) {
			// null out each direct pointer's data block and the direct pointer
			writeBlock(fisky, node->diskmap[i], null_array);
			super->block_freelist[node->diskmap[i] - DATA_OFFSET] = FREE;
#ifdef DEBUG
			printf("freed block %i\n", node->diskmap[i] - DATA_OFFSET);
#endif
			node->diskmap[i] = 0;
		}
	}
	
	/* if single indirection used, clear it */
	if (node->diskmap[12] != 0) {
		// load single indirect pointers and null out their data blocks
		readBlock(fisky, node->diskmap[12], single_array); 
		for (k = 0; k < 256; ++k)
			if (single_array[k] != -1) { // test if single indirect pointer exists
				writeBlock(fisky, single_array[k], null_array);
				super->block_freelist[single_array[k] - DATA_OFFSET] = FREE;
#ifdef DEBUG
				printf("freed block %i\n", single_array[k] - DATA_OFFSET);
#endif
			}
		reset_indirection(single_array);
		
		// clear single indirect data block
		writeBlock(fisky, node->diskmap[12], null_array);
		super->block_freelist[node->diskmap[12] - DATA_OFFSET] = FREE;
#ifdef DEBUG
		printf("freed block %i\n", node->diskmap[12] - DATA_OFFSET);
#endif
		++indirection_data_blocks;
		
		// clear single indirect diskmap value
		node->diskmap[12] = 0;
	}
		
	/* if double indirection used, clear it */
	if (node->diskmap[13] != 0) {
		// load double indirect data block pointers
		readBlock(fisky, node->diskmap[13], double_array); 
		
		// load single indirect pointers and null out their data blocks
		for (j = 0; j < 256; ++j) {
			if (double_array[j] != -1) { // test if double indirect pointer exists
				readBlock(fisky, double_array[j], single_array);
				
				for (k = 0; k < 256; ++k)
					if (single_array[k] != -1) { // test if single indirect pointer exists
						writeBlock(fisky, single_array[k], null_array);
						super->block_freelist[single_array[k] - DATA_OFFSET] = FREE;
#ifdef DEBUG
						printf("freed block %i\n", single_array[k] - DATA_OFFSET);
#endif
					}
				reset_indirection(single_array);
				
				// clear single indirect data block
				writeBlock(fisky, double_array[j], null_array);
				super->block_freelist[double_array[k] - DATA_OFFSET] = FREE;
#ifdef DEBUG
				printf("freed block %i\n", double_array[k] - DATA_OFFSET);
#endif
				++indirection_data_blocks;
			}
		}
		// clear  double indirect data block
		reset_indirection(double_array);
		writeBlock(fisky, node->diskmap[13], null_array);
		super->block_freelist[node->diskmap[13] - DATA_OFFSET] = FREE;
#ifdef DEBUG
		printf("freed block %i\n", node->diskmap[13] - DATA_OFFSET);
#endif
		++indirection_data_blocks;
		
		// clear double indirect diskmap value
		node->diskmap[13] = 0;
	}
	
	/* if  triple indirection used, clear it */
	if (node->diskmap[14] != 0) {
		// load triple indirect data block pointers
		readBlock(fisky, node->diskmap[14], triple_array);
		
		// load double indirect data block pointers
		for (i = 0; i < 256; ++i) {
			if (triple_array[i] != -1) { // test if triple indirect pointer exists
				readBlock(fisky, triple_array[i], double_array);
				
				// load single indirect pointers and null out their data blocks
				for (j = 0; j < 256; ++j) {
					if (double_array[j] != -1) { // test if double indirect pointer exists
						readBlock(fisky, double_array[j], single_array);
						
						for (k = 0; k < 256; ++k)
							if (single_array[k] != -1) { // test if single indirect pointer exists
								writeBlock(fisky, single_array[k], null_array);
								super->block_freelist[single_array[k] - DATA_OFFSET] = FREE;
#ifdef DEBUG
								printf("freed block %i\n", single_array[k] - DATA_OFFSET);
#endif
								
							}
						reset_indirection(single_array);
						
						// clear single indirect data block
						writeBlock(fisky, double_array[j], null_array);
						super->block_freelist[double_array[k] - DATA_OFFSET] = FREE;
#ifdef DEBUG
						printf("freed block %i\n", double_array[k] - DATA_OFFSET);
#endif
						++indirection_data_blocks;
					}
				}
				// clear  double indirect data block
				reset_indirection(double_array);
				writeBlock(fisky, triple_array[i], null_array);
				super->block_freelist[triple_array[i] - DATA_OFFSET] = FREE;
#ifdef DEBUG
				printf("freed block %i\n", triple_array[i] - DATA_OFFSET);
#endif
				++indirection_data_blocks;
			}
		}
		// clear  triple indirect data block
		reset_indirection(triple_array);
		writeBlock(fisky, node->diskmap[14], null_array);
		super->block_freelist[node->diskmap[14] - DATA_OFFSET] = FREE;
#ifdef DEBUG
		printf("freed block %i\n", node->diskmap[14] - DATA_OFFSET);
#endif
		++indirection_data_blocks;
		
		// clear triple indirect diskmap value
		node->diskmap[14] = 0;
	}
	
#ifdef DEBUG
	printf("freed indirection data blocks = %i\n", indirection_data_blocks);
	printf("total freed blocks = %i\n", node->filesize/BLOCK_SIZE + indirection_data_blocks);
#endif
	
	/** RESET INODE node TO DEFAULT UNUSED VALUES **/
	/** RESET INODE node TO DEFAULT UNUSED VALUES **/
	
	/* calculates the block number the inode resides in 
		and its index within that block */
	inode_block_num = (inode_index / 14) + INODE_OFFSET;
	inode_start_index = (inode_index % 14);
	
	/* reset p_inode_buffer to beginning of inode index */
	p_inode_buffer = &inode_buffer[inode_start_index*INODE_SIZE];
	
	/* update the inode attributes */
	node->filecode = DIRECTORY;
	node->filesize = BLOCK_SIZE; 
	node->inode_num = inode_index;
	
	/* loads the struct onto the inode buffer */
	*p_inode_buffer++ = node->filecode;
	*p_inode_buffer++ = node->filesize;
	*p_inode_buffer++ = node->inode_num;
	
	/* loads the diskmap content onto the buffer to be written to the fisk */
	for (i = 0; i < 15; ++i)
		*p_inode_buffer++ = 0; // resetting buffer values to 0
	
	/* writes the inode buffer onto fisk */
	writeBlock(fisky, inode_block_num, inode_buffer);
	
	/* updates the total free data space */
	super->free_data_space = super->free_data_space + node->filesize + indirection_data_blocks*BLOCK_SIZE;
	write_super_blocks();
	
	return 0;
}

/* @desc: grabs a free data block from the data block free list and sets it
 *	to occupied.
 * @rets: 0 if successful, -1 otherwise.
 */
static int get_free_data_block() {
	int i, result;
	int block_index;
	
	result = 0; // flag for free block found
	/* searches the block freelist for a free data block
		and return its index */
	for (i = 0; i < data_block_size; i++) {
		if (super->block_freelist[i] == 0) {
			block_index = i;
			super->block_freelist[i] = OCCUPIED;
			result = 1; // set flag for found!
			break;
		}
	}
	
	if (result == 1) {
		write_super_blocks();
		return block_index + DATA_OFFSET;
	} else
		return -1;
}

/* @desc: creates a new file, setting up its inode and reserving
 *	data blocks for it.
 * @rets: 0 if successful, -1 otherwise.
 */
int create_file(int filesize, char* filename) {
	int filecode = FILE;
	int inode_index;
	int dir_block_index;
	int* int_data_buffer = (int*)data_buffer; // insert ints into data_buffer
	char* char_data_buffer = (char*)data_buffer; // insert chars into data_buffer
	int i, j, k;
	int filecount;
	
	/*** CHECKING IF FILE ALREADY EXISTS ***/
	/*** CHECKING IF FILE ALREADY EXISTS ***/
	
	// search for file name. +2 because of . and ..
	for (i = 0; i < (curr_dir->filecount)+2; i++) {

		if (strcmp(filename, curr_dir->filename[i]) == 0) {
			printf("Cannot create file '%s': File exists\n", filename);
			return -1;
		}
	}
	
	// if filesize is equal to 0, then EFFS still reserves a single data block for the file
	if (filesize == 0) {
		filesize = BLOCK_SIZE;
	} else if (filesize < 0) {
		printf("Cannot create a file of negative size!\n");
		return -1;
	}
	
	read_inode(curr_dir->inode_num[0]); // sets the global node to the current directory's node
	dir_block_index = node->diskmap[0]; // gets the block number of the directory table
	readBlock(fisky, dir_block_index, data_buffer);
	
	filecount = int_data_buffer[0];
	if (filecount >= MAX_FILECOUNT) {
		printf("Maximum number of files per directory reached!\n");
		return -1;
	}
	filecount++;
	
	inode_index = get_free_inode();	
	if (inode_index == -1) {
		printf("Maximum number of files reached!\n");
		return -1;
	}
	
	int_data_buffer[0] = filecount;
	curr_dir->filecount = filecount;
	/* searches the directory table for a free spot */
	for (i = 2; i < 51; i++) {
		if (int_data_buffer[i] == -1) {
			int_data_buffer[i] = inode_index;
			curr_dir->inode_num[i-1] = inode_index;
			break;
		}
	}
	
	i--; // offset the filecount index in the data_buffer

	/* null out the current file in that position of the buffer */
	for (k = 0, j = (204 + (i*10)); j < (204 + (i*10) + 10); j++, k++) {
		char_data_buffer[j] = '\0';
		curr_dir->filename[i][k] = '\0';
	}
	
	/* write in the filename to that buffer position */
	for (k = 0, j = (204 + (i*10)); j < (204 + (i*10) + strlen(filename)); j++, k++) {
		curr_dir->filename[i][k] = filename[k];
		char_data_buffer[j] = filename[k];	
	}	
	
	writeBlock(fisky, dir_block_index, data_buffer);
	
	/* after updating the directory table, write contents to fisk */
	
	read_inode(inode_index);
	add_inode(inode_index, filecode, filesize);
	
	return 0;
}

/* @desc: creates a new directory, setting up its inode and reserving
 *	data blocks for it.
 * @rets: 0 if successful, -1 otherwise.
 */
int create_dir(char* dirname) {
	int i, j, k;
	int* int_data_buffer = (int*)data_buffer;
	char* char_data_buffer = (char*)data_buffer;
	int inode_index;
	int dir_block_index;
	int filecode = DIRECTORY;
	int filesize = BLOCK_SIZE;
	int filecount;
	
	/*** CHECKING IF DIRECTORY ALREADY EXISTS ***/
	/*** CHECKING IF DIRECTORY ALREADY EXISTS ***/
	
	// +2 because of . and ..
	for (i = 0; i < (curr_dir->filecount)+2; i++) {

		if (strcmp(dirname, curr_dir->filename[i]) == 0) {
			printf("Cannot create directory '%s': File exists\n", dirname);
			return -1;
		}
	}
	
	/*** UPDATE CURRENT DIRECTORY'S TABLE TO ADD THIS NEW DIRECTORY ***/
	/*** UPDATE CURRENT DIRECTORY'S TABLE TO ADD THIS NEW DIRECTORY ***/	
	
	read_inode(curr_dir->inode_num[0]); // sets the global node to the current directory's node
	dir_block_index = node->diskmap[0]; // gets the block number of the directory table
	readBlock(fisky, dir_block_index, data_buffer);
	
	filecount = int_data_buffer[0];
	if (filecount >= MAX_FILECOUNT) {
		printf("Maximum number of files per directory reached!\n");
		return -1;
	}
	filecount++;
	
	inode_index = get_free_inode();	
	if (inode_index == -1) {
		printf("Maximum number of files reached!\n");
		return -1;
	}

	int_data_buffer[0] = filecount;
	curr_dir->filecount = filecount;
	/* searches the directory table for a free spot */
	for (i = 2; i < 51; i++) {
		if (int_data_buffer[i] == -1) {
			int_data_buffer[i] = inode_index;
			curr_dir->inode_num[i-1] = inode_index;
			break;
		}
	}
	
	i--; // offset the filecount index in the data_buffer
	
	/* null out the current file in that position of the buffer */
	for (k = 0, j = (204 + (i*10)); j < (204 + (i*10) + 10); j++, k++) {
		char_data_buffer[j] = '\0';
		curr_dir->filename[i][k] = '\0';
	}
	
	/* write in the directory name to that buffer position */
	for (k = 0, j = (204 + (i*10)); j < (204 + (i*10) + strlen(dirname)); j++, k++) {
		curr_dir->filename[i][k] = dirname[k];
		char_data_buffer[j] = dirname[k];
	}

	writeBlock(fisky, dir_block_index, data_buffer);
	
	/* after updating the directory table, write contents to fisk */
	read_inode(inode_index);
	add_inode(inode_index, filecode, filesize);
	
	/*** CREATE THE NEW DIRECTORY'S TABLE ***/
	/*** CREATE THE NEW DIRECTORY'S TABLE ***/
	
	dir_block_index = node->diskmap[0];

	/* temp_filename is a placeholder for filename passed in */
	char* temp_filename = ".";
	new_dir->filecount = 0;
	new_dir->inode_num[0] = inode_index;
	/* write string temp_filename to current directory's filename */
	for (i = 0; i < strlen(temp_filename); ++i)
		new_dir->filename[0][i] = temp_filename[i];
	
	temp_filename = "..";
	new_dir->inode_num[1] = curr_dir->inode_num[0];
	/* write string temp_filename to current directory's filename */
	for (i = 0; i < strlen(temp_filename); ++i)
		new_dir->filename[1][i] = temp_filename[i];

	/* setting inode numbers for unused spots as -1 (since not in use yet) */
	for (i = 2; i < 50; i++) {
		new_dir->inode_num[i] = -1;
	}

	temp_filename = "$effs$"; // so that we can keep track of empty spots in directory

	/* loads strings for unused spots to our temp_filename and then into current dir's filename[index] */
	for (i = 2; i < 50; i++)
		for (j = 0; j < strlen(temp_filename); ++j)
			new_dir->filename[i][j] = temp_filename[j];

	/* loading filecount as first thing in data buffer (0-byte to 3-byte) */
	int_data_buffer[0] = new_dir->filecount;
	
	/* loading inode numbers as next 1 to 51 things in data buffer (4-byte to 203-byte) */
	for (i = 1; i < 51; i++) {
		int_data_buffer[i] = new_dir->inode_num[i-1];
	}
	  
	/* putting the 9-char strings (10 including null) to data buffer. 50 of these (204-byte to 703-byte) */
	k = 0;
	for (i = 204; i < 254; i++, k++)
		for (j = 0; j < FILE_NAME_SIZE; j++)
			char_data_buffer[i + j + k*9] = new_dir->filename[i-204][j];

	/* nulling out the rest of the data buffer that is unused (704-byte to 1023-byte) */
	for (i = 704; i < BUFFER_SIZE; i++)
		char_data_buffer[i] = '\0';
	
	writeBlock(fisky, dir_block_index, data_buffer);

	return 0;	
}

/* @desc: general create function that calls either create_file() or
 *	create_dir() depending on flag.
 * @rets: 0 if successful, -1 otherwise.
 */
int effs_create(char* name, int flag, int filesize) {
	int result;
	
	if (flag == FILE) {
		result = create_file(filesize, name);
		return result;
	}
	else if (flag == DIRECTORY) {
		result = create_dir(name);
		return result;
	}
	else {
		printf("Invalid file type! File = 0 and Directory = 1\n");
		return -1;
	}
}

/* @desc: changes current directory to a directory in current directory's
 *	(inode #, filename) tuples list.
 * @rets: 0 if successful, -1 otherwise.
 */
void change_dir(char* dirname) {
	int i, j, k, dir_block_index;
	int cd_inode_num = -1;                      	
	
	// +2 because of . and ..
	for (i = 0; i < (curr_dir->filecount)+2; i++) {

		if (strcmp(dirname, curr_dir->filename[i]) == 0) {
			cd_inode_num = curr_dir->inode_num[i];
			break;
		}
	}
	
	// setting root directory as current directory
	if (strcmp(dirname,"root") == 0) {
		cd_inode_num = 0;
	}
	
	// reads the new current directory's inode struct into our global node variable
	read_inode(cd_inode_num);
	
	// if the new current directory does not exist or if it's a file, then error!
	if (cd_inode_num == -1 || node->filecode == FILE) {
		printf("Directory does not exist!\n");	
	} else if (i == 0) {
		printf("You have changed your current directory to current directory!\n");
	} else if (i == 1 && curr_dir->inode_num[0] == 0) {
		printf("Root has no parent..\n");	
	} else {
		// setting the new current directory's block location
		dir_block_index = node->diskmap[0];
		
		// now loading new current directory struct into data_buffer
		readBlock(fisky, dir_block_index, data_buffer);
		int* int_data_buffer = (int*)data_buffer; // reading from data_buffer into directory struct
		char* char_data_buffer = (char*)data_buffer; // reading from data_buffer into directory struct
		
		// now loading new current directory struct from data_buffer into curr_dir
		curr_dir->filecount = int_data_buffer[0];
		for (i = 0; i < curr_dir->filecount+2; i++) {
			curr_dir->inode_num[i] = int_data_buffer[i+1];
		}
		k = 0;
		for (i = 204; i < 254; i++, k++) {
			for (j = 0; j < FILE_NAME_SIZE; j++) {
				curr_dir->filename[i-204][j] = char_data_buffer[i + j + k*9];
			}
		}
	}
}

/* @desc: Gets the current working directory and prints it to stdout.
 * @rets: 0 if succeeded. -1 otherwise.
 */
int get_curr_dir() {
	char* path_name = (char*)malloc(sizeof(char)*100);
	char* st1 = (char*)malloc(sizeof(char)*100);
	char* st2 = (char*)malloc(sizeof(char)*100);
	int i, j, k;
	int parent_node;
	int dir_block_index;
	int curr_dir_inode_num = curr_dir->inode_num[0];
	
	// dummy temp_dir struct
	directory_t temp_dir;
	temp_dir = (directory_t)malloc(sizeof(struct directory));
	temp_dir->inode_num = (int*)malloc(sizeof(int)*50);
	temp_dir->filename = (char**)malloc(sizeof(char*)*50);
	for (i = 0; i < 50; ++i)
	{
		temp_dir->filename[i] = (char*)malloc(sizeof(char)*FILE_NAME_SIZE);
		for (j = 0; j < FILE_NAME_SIZE; ++j)
			temp_dir->filename[i][j] = '\0';
	}
	
	// copies curr_dir struct to temp_dir struct
	temp_dir->filecount = curr_dir->filecount;
	for (i = 0; i < curr_dir->filecount+2; i++) {
		temp_dir->inode_num[i] = curr_dir->inode_num[i];		
		for (k = 0; k < strlen(curr_dir->filename[i]); ++k)
			temp_dir->filename[i][k] = curr_dir->filename[i][k];
	}
	
	// if the 1st inode number is 0, then current directory is root
	path_name = "/root";
	if (curr_dir_inode_num == 0) {
		printf("%s\n", path_name);
		return 0;
	}
	
	
	int done = 0;
	while (done != 1) {
		// otherwise read the parent of current directory into global node struct
		parent_node = temp_dir->inode_num[1];
		read_inode(parent_node);
		// find out where the parent node stores its directory table
		dir_block_index = node->diskmap[0];
	
		// reads the block where the parent directory table is stored
		readBlock(fisky, dir_block_index, data_buffer);
		int* int_data_buffer = (int*)data_buffer; // reading from data_buffer into directory struct
		char* char_data_buffer = (char*)data_buffer; // reading from data_buffer into directory struct
	
		// loading the directory struct into the dummy directory struct
		temp_dir->filecount = int_data_buffer[0];
		for (i = 0; i < temp_dir->filecount+2; i++) {
			temp_dir->inode_num[i] = int_data_buffer[i+1];
		}
		k = 0;
		for (i = 204; i < 254; i++, k++) {
			for (j = 0; j < FILE_NAME_SIZE; j++) {
				temp_dir->filename[i-204][j] = char_data_buffer[i + j + k*9];
			}
		}
		for (i = 0; i < temp_dir->filecount+2; i++) {
			if (temp_dir->inode_num[i] == curr_dir_inode_num) {
				strcpy(st1, temp_dir->filename[i]);
				strcat(st1, "/");
				strcat(st1, st2);
				strcpy(st2, st1);
				break;
			}
		}
		curr_dir_inode_num = temp_dir->inode_num[0];
		if (curr_dir_inode_num == 0)
			done = 1;
	}
	// print working directory
	printf("%s/%s\n", path_name, st1);

	// freeing mallocs
	for (i = 0; i < 50; ++i) {
		free(temp_dir->filename[i]);
		free(temp_dir->inode_num);
	}

	free(temp_dir);

	return 0;
}

/* @desc: Deletes file specified by filename. Nulls out the diskmap of inode
 *	of file to be deleted and updates file's parent directory (inode #, fname)
 *	tuple chart.
 * @rets: 0 if succeeded. -1 otherwise.
 */
int effs_delete(char* filename) {
	int i, j, k;
	int d_inode_num;
	int file_exists = 0;
	int dir_block_index;
	char* char_data_buffer = (char*)data_buffer;
	int* int_data_buffer = (int*)data_buffer;
	int table_index;

	/** TEST FOR ERRORS **/
	/** TEST FOR ERRORS **/
	
	if (strcmp(filename, ".") == 0) {
		printf("You're in this directory.. cannot delete!\n");
		return -1;
	} else if (strcmp(filename, "..") == 0) {
		printf("Directory is not empty.. cannot delete!\n");
		return -1;
	} else {
		// check if file exists in current directory.
		for (i = 2; i < (curr_dir->filecount)+2; i++) { // +2 because of . and ..
			if (strcmp(filename, curr_dir->filename[i]) == 0) {
				d_inode_num = curr_dir->inode_num[i];
				++file_exists;
				break;
			}
		}
		if (!file_exists) {
			fprintf(stderr, "File not found.\n");	
			return -1;
		}
	
#ifdef DEBUG
		printf("inode # of file = %i\n", d_inode_num);
#endif
		
		/** LOAD AND REMOVE INODE OF FILE TO DELETE **/
		/** LOAD AND REMOVE INODE OF FILE TO DELETE **/
		
		remove_inode(d_inode_num); // will null data in inode's diskmap
		
		// flag inode in freelist as FREE
		super->inode_freelist[d_inode_num] = FREE;
		write_super_blocks();
		
		/** UPDATE CURRENT DIRECTORY'S (INODE #, FILENAME) TABLE **/
		/** UPDATE CURRENT DIRECTORY'S (INODE #, FILENAME) TABLE **/
	
		// get index of deleted file in current directory's (inode #, filename) table
		for (i = 2; i < (curr_dir->filecount)+2; i++) {
			if (d_inode_num == curr_dir->inode_num[i]) {
				table_index = i;
				break;
			}
		}

		if (table_index == 49) {
			// if table_index is last subfile, set its inode number to -1 and rename its filename as $effs$
			curr_dir->inode_num[table_index] = -1;
			for (k = 0; k < FILE_NAME_SIZE; ++k)
				curr_dir->filename[table_index][k] = empty_file[k];
		} else {
			// otherwise, shift all files down 1 and set and rename last subfile as -1 and $effs$
			for (i = table_index; i < (curr_dir->filecount)+2; i++) {
				curr_dir->inode_num[i] = curr_dir->inode_num[i+1];
				for (k = 0; k < FILE_NAME_SIZE; ++k)
					curr_dir->filename[i][k] = curr_dir->filename[i+1][k];
			}
			// filecount + 1 since we've already subtracted it
			curr_dir->inode_num[(curr_dir->filecount+1)] = -1;
			for (k = 0; k < strlen(empty_file); ++k)
				curr_dir->filename[(curr_dir->filecount+1)][k] = empty_file[k];
			for (k = strlen(empty_file); k < FILE_NAME_SIZE; ++k)
				curr_dir->filename[(curr_dir->filecount+1)][k] = '\0';
		}
	
		curr_dir->filecount--;
		
		// load current directory's inode to be written to disk
		read_inode(curr_dir->inode_num[0]);
		dir_block_index = node->diskmap[0];
		
		/* loading filecount as first thing in data buffer (0-byte to 3-byte) */
		int_data_buffer[0] = curr_dir->filecount;
		
		/* loading inode numbers as next 1 to 51 things in data buffer (4-byte to 203-byte) */
		for (i = 1; i < 51; i++) {
			int_data_buffer[i] = curr_dir->inode_num[i-1];
		}
		  
		/* putting the 9-char strings (10 including null) to data buffer. 50 of these (204-byte to 703-byte) */
		k = 0;
		for (i = 204; i < 254; i++, k++)
			for (j = 0; j < 10; j++)
				char_data_buffer[i + j + k*9] = curr_dir->filename[i-204][j];
	
		/* nulling out the rest of the data buffer that is unused (704-byte to 1023-byte) */
		for (i = 704; i < BUFFER_SIZE; i++)
			char_data_buffer[i] = '\0';
		
		writeBlock(fisky, dir_block_index, data_buffer);
	
		return 0;
	}
}

/* @desc: opens a file specified by filename for writing.
 * @rets: inode of the file if successfully opened. -1 otherwise.
 */
int effs_open(char* filename) {
	int i;
	int file_inode = -1;
	
	// searching for filename. +2 because of . and ..
	for (i = 0; i < (curr_dir->filecount)+2; i++) {
		if (strcmp(filename, curr_dir->filename[i]) == 0) {
			file_inode = curr_dir->inode_num[i];
			break;
		}
	}
	
	if (file_inode == -1) {
		printf("File does not exist!\n");
		return file_inode;
	} else {
#ifdef DEBUG
		printf("File @ inode %i\n", file_inode);
#endif
		return file_inode;
	}
}

/* @desc: Writes content from file_content to the data blocks in the diskmap
 * 	of inode_index. The amount to write, filesize, cannot be greater
 *	than that file's size.
 * @rets: 0 if succeeded, -1 otherwise.
 */
int effs_write(int inode_index, void *file_content, int filesize) {
	int i, j, k;
	int remaining;

	// remaining effs_write() when to stop writing
	remaining = filesize;
	
	// we will be transferring data by the byte (char)
	char* p_file_content = (char*)file_content;

	// read inode meta data from specified file index
	read_inode(inode_index);
	
	// checking if the file content will fit into the available data space
	if (filesize > node->filesize) {
		fprintf(stderr, "Segmentation fault: cannot overwrite past filesize.\n"); 	
		return -1;
	}
	
	/** WRITING FILE CONTENT TO READ INODE node's DISKMAP **/
	/** WRITING FILE CONTENT TO READ INODE node's DISKMAP **/
	
	/* write first 12 data blocks of current node (direct) */
	for (i = 0; i < 12; ++i) {
		if (node->diskmap[i] != 0) {
			// write a block of content to each direct pointer's data block
			writeBlock(fisky, node->diskmap[i], p_file_content);
			p_file_content = p_file_content + BLOCK_SIZE;
			remaining = remaining - BLOCK_SIZE;
			printf("wrote block %i\n", node->diskmap[i]);
			if (remaining <= 0)
				return 0;
		}
	}
	
	/* if single indirection used, write to pointed blocks */
	if (node->diskmap[12] != 0) {
		// load single indirect pointers and write to their data blocks
		readBlock(fisky, node->diskmap[12], single_array); 
		for (k = 0; k < 256; ++k)
			if (single_array[k] != -1) { // test if single indirect pointer exists
				// write a block of content to single_array pointers
				writeBlock(fisky, single_array[k], p_file_content);
				p_file_content = p_file_content + BLOCK_SIZE;
				remaining = remaining - BLOCK_SIZE;
				printf("wrote block %i\n", single_array[k]);
				if (remaining <= 0)
					return 0;
			}
		reset_indirection(single_array);
	}

	/* if double indirection used, write to pointed blocks */
	if (node->diskmap[13] != 0) {
		// load double indirect data block pointers
		readBlock(fisky, node->diskmap[13], double_array); 
		
		// load single indirect pointers and write to their data blocks
		for (j = 0; j < 256; ++j) {
			if (double_array[j] != -1) { // test if double indirect pointer exists
				readBlock(fisky, double_array[j], single_array);
				
				for (k = 0; k < 256; ++k)
					if (single_array[k] != -1) { // test if single indirect pointer exists
						writeBlock(fisky, single_array[k], p_file_content);
						p_file_content = p_file_content + BLOCK_SIZE;
						remaining = remaining - BLOCK_SIZE;
						printf("wrote block %i\n", single_array[k]);
						if (remaining <= 0)
							return 0;
					}
				reset_indirection(single_array);
			}
		}
		reset_indirection(double_array);
	}
	
	/* if  triple indirection used, clear it */
	if (node->diskmap[14] != 0) {
		// load triple indirect data block pointers
		readBlock(fisky, node->diskmap[14], triple_array);
		
		// load double indirect data block pointers
		for (i = 0; i < 256; ++i) {
			if (triple_array[i] != -1) { // test if triple indirect pointer exists
				readBlock(fisky, triple_array[i], double_array);
				
				// load single indirect pointers and write to their data blocks
				for (j = 0; j < 256; ++j) {
					if (double_array[j] != -1) { // test if double indirect pointer exists
						readBlock(fisky, double_array[j], single_array);
						
						for (k = 0; k < 256; ++k)
							if (single_array[k] != -1) { // test if single indirect pointer exists
								writeBlock(fisky, single_array[k], p_file_content);
								p_file_content = p_file_content + BLOCK_SIZE;
								remaining = remaining - BLOCK_SIZE;
								printf("wrote block %i\n", single_array[k]);
								if (remaining <= 0)
									return 0;								
							}
						reset_indirection(single_array);
					}
				}
				reset_indirection(double_array);
			}
		}
		reset_indirection(triple_array);
	}
	return 0;
}

/* @desc: Helper function of effs_read() that transfers
 *	from one buffer stream to another.
 * @rets: 0 if succeeded
 */
int print_data_buffer(char* p_data_buffer, int amount) {
	int i;
	
	for (i = 0; i < amount; ++i)
		printf("%c", p_data_buffer[i]);
	printf("\n");
	return 0;
}

/* @desc: Reads content from the data blocks in the diskmap of inode_index
 * 	to data_buffer and prints it. The amount to read, filesize, cannot be
 *	greater than that file's size.
 * @rets: 0 if succeeded, -1 otherwise.
 */
int effs_read(char* filename, void *file_content, int filesize) {
	int i, j, k;
	int inode_index;
	int file_exists = 0;
	int remaining;

	// remaining tells effs_read() when to stop reading
	remaining = filesize;
	
	// we will be transferring data by the byte (char)
	char* p_file_content = (char*)file_content;
	char* p_data_buffer = (char*)data_buffer;
	
	// find the file index from file name (node should be current directory)
	// check if file exists in current directory.
	for (i = 2; i < (curr_dir->filecount)+2; i++) { // +2 because of . and ..
		if (strcmp(filename, curr_dir->filename[i]) == 0) {
			inode_index = curr_dir->inode_num[i];
			++file_exists;
			break;
		}
	}
	if (!file_exists) {
		fprintf(stderr, "File not found.\n");	
		return -1;
	}

	// read inode meta data from specified file index
	read_inode(inode_index);
	
	// checking if the file content exceeds read size
	if (filesize > node->filesize) {
		fprintf(stderr, "Segmentation fault: cannot read past file size.\n"); 	
		return -1;
	}
	
	/** READING FILE CONTENT node's DISKMAP TO DATA_BUFFER & PRINT**/
	/** READING FILE CONTENT node's DISKMAP TO DATA_BUFFER & PRINT**/
	
	/* read first 12 data blocks of current node (direct) */
	for (i = 0; i < 12; ++i) {
		if (node->diskmap[i] != 0) {
			// read a block of content to each direct pointer's data block
			printf("reading block %i\n", node->diskmap[i]);
			readBlock(fisky, node->diskmap[i], p_data_buffer);
			print_data_buffer(p_data_buffer, BLOCK_SIZE);
			p_file_content = p_file_content + BLOCK_SIZE;
			remaining = remaining - BLOCK_SIZE;

			if (remaining <= 0)
				return 0;
		}
	}
	
	/* if single indirection used, read from pointed blocks */
	if (node->diskmap[12] != 0) {
		// load single indirect pointers and read their data blocks
		readBlock(fisky, node->diskmap[12], single_array); 
		for (k = 0; k < 256; ++k)
			if (single_array[k] != -1) { // test if single indirect pointer exists
				// read a block of content from single_array pointers
				printf("reading block %i\n", single_array[k]);
				readBlock(fisky, single_array[k], p_data_buffer);
				print_data_buffer(p_data_buffer, BLOCK_SIZE);
				p_file_content = p_file_content + BLOCK_SIZE;
				remaining = remaining - BLOCK_SIZE;

				if (remaining <= 0)
					return 0;
			}
		reset_indirection(single_array);
	}

	/* if double indirection used, write to pointed blocks */
	if (node->diskmap[13] != 0) {
		// load double indirect data block pointers
		readBlock(fisky, node->diskmap[13], double_array); 
		
		// load single indirect pointers and read their data blocks
		for (j = 0; j < 256; ++j) {
			if (double_array[j] != -1) { // test if double indirect pointer exists
				readBlock(fisky, double_array[j], single_array);
				
				for (k = 0; k < 256; ++k)
					if (single_array[k] != -1) { // test if single indirect pointer exists
						printf("reading block %i\n", single_array[k]);
						readBlock(fisky, single_array[k], p_data_buffer);
						print_data_buffer(p_data_buffer, BLOCK_SIZE);
						p_file_content = p_file_content + BLOCK_SIZE;
						remaining = remaining - BLOCK_SIZE;
	
						if (remaining <= 0)
							return 0;
					}
				reset_indirection(single_array);
			}
		}
		reset_indirection(double_array);
	}
	
	/* if  triple indirection used, clear it */
	if (node->diskmap[14] != 0) {
		// load triple indirect data block pointers
		readBlock(fisky, node->diskmap[14], triple_array);
		
		// load double indirect data block pointers
		for (i = 0; i < 256; ++i) {
			if (triple_array[i] != -1) { // test if triple indirect pointer exists
				readBlock(fisky, triple_array[i], double_array);
				
				// load single indirect pointers and read their data blocks
				for (j = 0; j < 256; ++j) {
					if (double_array[j] != -1) { // test if double indirect pointer exists
						readBlock(fisky, double_array[j], single_array);
						
						for (k = 0; k < 256; ++k)
							if (single_array[k] != -1) { // test if single indirect pointer exists
								printf("reading block %i\n", single_array[k]);
								readBlock(fisky, single_array[k], p_data_buffer);
								print_data_buffer(p_data_buffer, BLOCK_SIZE);
								p_file_content = p_file_content + BLOCK_SIZE;
								remaining = remaining - BLOCK_SIZE;

								if (remaining <= 0)
									return 0;
							}
						reset_indirection(single_array);
					}
				}
				reset_indirection(double_array);
			}
		}
		reset_indirection(triple_array);
	}
	return 0;
}

/* @desc: syncs open file descriptor fildes to disk.
 * @rets: 0 if succeeded. Should succeed.
 */
int effs_sync(int fildes) {
	syncDisk();
	return 0;
}

/* @desc: syncs file descriptor fildes and closes it (done in shell).
 * @rets: 0 if succeeded. Should succeed.
 */
int effs_close(int fildes) {
	effs_sync(fildes);
	return 0;
}

/* @desc: nulls out indirection array.
 */
static void reset_indirection(int* array) {
	int i;
	for (i = 0; i < 256; i++) {
		array[i] = -1;	
	}
}


