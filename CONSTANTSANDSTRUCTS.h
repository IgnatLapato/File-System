#ifndef CONSTANTSANDSTRUCTS_H
#define CONSTANTSANDSTRUCTS_H
#include <memory.h>
#include <stdlib.h>
#include "disk_emu.h"


//Constants:
#define BLOCK_SIZE 1024
#define BLOCK_COUNT 1024
#define TOTAL_DATABLOCKS 1011
#define TOTAL_INODES 100
#define TOTAL_INODE_POINTERS 12
#define MAXFILENAME 16
#define SUPERBLOCK_DATABLOCK_INDEX 0 //1 block 
#define INODETABLE_DATABLOCK_INDEX 1 //8 blocks 
#define FREE_BITMAP_DATABLOCK_INDEX 9 //1 block 
#define ROOT_DIR_DATABLOCK_INDEX 10 //3 blocks
#define FILES_DATABLOCK_INDEX 13 //1011 blocks

/* ----------------------------------------------------------DATA STRUCTURES -------------------------------------------------------------*/


//Super Block
typedef struct {
    uint64_t magic;
    int block_size;
    int fs_size;
    int inode_table_length;
    int root_dir_inode_index;
} super_block_t;

void super_block_init(super_block_t *super_block) {
    super_block->magic = 0;
    super_block->block_size = -1;
    super_block->inode_table_length = -1;
    super_block->fs_size = -1;
    super_block->root_dir_inode_index = -1;
}


//I-Node
typedef struct {
    int mode;
    int link_count;
    int uid;
    int gid;
    int size;
    int data_pointrs[TOTAL_INODE_POINTERS];
    int indirectPointer;
} inode_t;

void inode_init(inode_t *inode) {
    inode->mode = 0;
    inode->link_count = 1;
    inode-> uid = 0;
    inode->gid = 0;
    inode->size = 0;
    for (int i = 0; i < TOTAL_INODE_POINTERS; ++i) {
        inode->data_pointrs[i] = -1;
    }
    inode->indirectPointer = -1;
}


//I-Node Table
typedef struct {
    inode_t nodes[TOTAL_INODES];
    char free[TOTAL_INODES];
} inode_table_t;

void inode_table_init(inode_table_t* inode_table) {//Table is initialized by calling an inintializer on every i-node
    for (int i = 0; i < TOTAL_INODES; ++i) {
        inode_init(&inode_table->nodes[i]);
        inode_table->free[i] = 1;
    }
}

void inode_table_setFree(inode_table_t* inode_table, int index) {//1 signifies a free i-node, 0 means in use
    inode_table->free[index] = 1;
}

void inode_table_setUsed(inode_table_t* inode_table, int index) {
    inode_table->free[index] = 0;
}

int inode_table_isFree(inode_table_t* inode_table, int index) {//Returns 1 -> free, 0 -> in use
    return inode_table->free[index] == 1;
}

int inode_table_isUsed(inode_table_t* inode_table, int index) {//Returns 1 -> in use, 0 ->free
    return inode_table->free[index] == 0;
}

int inode_table_nextFree(inode_table_t* inode_table) {
    for (int i = 0; i < TOTAL_INODES; ++i) {
        if (inode_table_isFree(inode_table, i)) {
            return i;
        }
    }
    return -1;
}


//Indirect Block
typedef struct {
    int pointers[BLOCK_SIZE / sizeof(int)];
} indirect_block_t;

void indirect_block_init(indirect_block_t* indirect_block) {//Initializer sets all blocks to -1, meaning they are unused
    for (int i = 0; i < BLOCK_SIZE / sizeof(int); ++i) {
        indirect_block->pointers[i] = -1;
    }
}

int indirect_block_isFree(indirect_block_t* indirect_block, int index) {//Returns 1 -> free, 0 -> in use
    return indirect_block->pointers[index] == -1;
}

int indirect_block_nextFree(indirect_block_t* indirect_block) {
    for (int i = 0; i < BLOCK_SIZE / sizeof(int); ++i) {
        if (indirect_block_isFree(indirect_block, i)) {
            return i;
        }
    }
    return -1;
}


//Bitmap
typedef struct {  //1 Bitmap char in array for every data block on disk 
    char bits[TOTAL_DATABLOCKS];
} free_bitmap_t;

void free_bitmap_init(free_bitmap_t *free_bitmap) {
    for (int i = 0; i < TOTAL_DATABLOCKS; ++i) {
        free_bitmap->bits[i] = 1;
    }
}

void free_bitmap_setUsed(free_bitmap_t* free_bitmap, int index) {//Set char to 1 when block is free, 0 when in use
    free_bitmap->bits[index] = 0;
}

void free_bitmap_setFree(free_bitmap_t* free_bitmap, int index) {
    free_bitmap->bits[index] = 1;
}

int free_bitmap_isFree(free_bitmap_t* free_bitmap, int index) {//Return 1 -> Free, Return 0 -> in use
    return free_bitmap->bits[index] == 1;
}

int free_bitmap_isUsed(free_bitmap_t* free_bitmap, int index) {//Return 1-> in use. Retrurn 0 -> free
    return free_bitmap->bits[index] == 0;
}

int free_bitmap_nextFree(free_bitmap_t* free_bitmap) {//Return first index which is marked as a free data block
    for (int i = 0; i < TOTAL_DATABLOCKS; ++i) {
        if (free_bitmap_isFree(free_bitmap, i)) {
            return i;
        }
    }
    return -1;
}


//File Descriptor
typedef struct {// A data structure which has a pointer to the i-node of the file and has location to the read/write pointers
    int inode_index;
    inode_t* inode;
    int r_pointer;
    int w_pointer;
} file_descriptor_t;

void fd_init(file_descriptor_t* file_descriptor) {//Initializer sets pointers to -1 and provides and empty i-node
    file_descriptor->inode_index = -1;
    file_descriptor->w_pointer = -1;
    file_descriptor->r_pointer = -1;
    file_descriptor->inode = NULL;
}


//File Descriptor Table
typedef struct {//Puts together all file descriptors into an array
    file_descriptor_t table[TOTAL_INODES -1];
    char free[TOTAL_INODES -1];
} file_descriptor_table_t;

void fd_table_init(file_descriptor_table_t* file_descriptor_table) {//Initializer sets all to free
    for (int i = 0; i < TOTAL_INODES - 1; ++i) {
        fd_init(&file_descriptor_table->table[i]);
        file_descriptor_table->free[i] = 1;
    }
}

int fd_table_isFree(file_descriptor_table_t* file_descriptor_table, int index) {//Returns 1 -> free, 0 -> in use
    return file_descriptor_table->free[index] == 1;
}

int fd_table_isUsed(file_descriptor_table_t* file_descriptor_table, int index) {//Returns 1 -> in use, 0 -> free
    return file_descriptor_table->free[index] == 0;
}

void fd_table_setFree(file_descriptor_table_t* file_descriptor_table, int index) {
    file_descriptor_table->free[index] = 1;
}

void fd_table_setUsed(file_descriptor_table_t* file_descriptor_table, int index) {
    file_descriptor_table->free[index] = 0;
}

int fd_table_nextFree(file_descriptor_table_t* file_descriptor_table) {
    for (int i = 0; i < TOTAL_INODES - 1; ++i) {
        if (fd_table_isFree(file_descriptor_table, i)) {
            return i;
        }
    }
    return -1;
}

int fd_table_contains_inode(file_descriptor_table_t* file_descriptor_table, int inode_index) {// looks for and return the entry with the matching i-node index
    for (int i = 0; i < TOTAL_INODES - 1; ++i) {
        if (file_descriptor_table->table[i].inode_index == inode_index) {
            return i;
        }
    }
    return -1;
}


//Directory Entry
typedef struct {//Data sturcture for every entry in the diectory. Contains an i-node index corresponding to i-node representing said entry in i-node table. Also contains the file name as char array with filename limit imposed
    int inode_index;
    char name[MAXFILENAME];
} directory_entry_t;

void directory_entry_init(directory_entry_t* directory_entry) {//Initializer sets the name to 0 chars and an empty pointer to inode table
    memset(directory_entry->name, 0, MAXFILENAME);
    directory_entry->inode_index = -1;
}

void directory_entry_setName(directory_entry_t* directory_entry, char* name) {// Makes sure the file name does not exceed the set MAXFILENAME
    memcpy(directory_entry->name, name, MAXFILENAME);
}

void directory_entry_setInodeIndex(directory_entry_t *directory_entry, int inode_index) {
    directory_entry->inode_index = inode_index;
}


//Directory
typedef struct {//Data struct which contains all directory entries in an array of size TOTAL_INODES - 1 (since one entry is for the root directory itself). current_index is used to remember the last accessed directory
    directory_entry_t entries[TOTAL_INODES - 1]; 
    char free[TOTAL_INODES - 1];
    int currentIndex;
} directory_t;

void directory_init(directory_t *directory) {// Intitalizer creates all entries as free and sets current index to 0
    for (int i = 0; i < TOTAL_INODES - 1; ++i) {
        directory_entry_init(&directory->entries[i]);
        directory_entry_setInodeIndex(&directory->entries[i], i + 1);
        directory->free[i] = 1;
        directory->currentIndex = 0;
    }
}

void directory_setUsed(directory_t *directory, int index) {//0 means used, 1 means free
    directory->free[index] = 0;
}

void directory_setFree(directory_t *directory, int index) {
    directory->free[index] = 1;
}

int directory_isFree(directory_t *directory, int index) {//return 1 -> free, returns 0 -> in use
    return directory->free[index] == 1;
}

int directory_isUsed(directory_t *directory, int index) {//return 1 -> in use, returns 0 -> free
    return directory->free[index] == 0;
}

int directory_nextFree(directory_t *directory) {
    for (int i = 0; i < TOTAL_INODES - 1; ++i) {
        if (directory_isFree(directory, i)) {
            return i;
        }
    }
    return -1;
}

int directory_getInodeIndex(directory_t *directory, const char *name) {//Returns i-node index of a given directory entry
    for (int i = 0; i < TOTAL_INODES - 1; ++i) {
        if (strncmp(directory->entries[i].name, name, strlen(name)) == 0
            && directory->free[i] == 0) {
            return directory->entries[i].inode_index;
        }
    }
    return -1;
}

int directory_getEntryIndex(directory_t* directory, const char *name) {//Returns directory table index of a given directory table entry
    for (int i = 0; i < TOTAL_INODES - 1; ++i) {
        if (strncmp(directory->entries[i].name, name, strlen(name)) == 0
                && directory->free[i] == 0) {
            return i;
        }
    }
    return -1;
}


/* ----------------------------------------------------------HELPER FUNCTIONS-------------------------------------------------------------*/
size_t byteToBlock(size_t size) {
    return ((size + BLOCK_SIZE - 1) / BLOCK_SIZE);
}

//This is a helper method which is used to get the exact amount of bytes from all data blocks to which this data belongs. All of the byte are kept in a temporary bufffer.
void read_from_disk(int start_address, size_t size, void* buffer) {
    size_t blockCount = byteToBlock(size);
    void* tempBuffer = malloc(blockCount * BLOCK_SIZE);
    memset(tempBuffer, 0, blockCount * BLOCK_SIZE);
    read_blocks(start_address, (int) blockCount, tempBuffer);
    memcpy(buffer, tempBuffer, size);
    free(tempBuffer);
}


#endif 
