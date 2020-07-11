#include "sfs_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include <signal.h>
#include "disk_emu.h"
#include "CONSTANTSANDSTRUCTS.h"
#define MY_DISK "Laptop_Ignat_sfs_disk.disk"
inode_table_t *inode_table = NULL;
directory_t *root_dir = NULL;
file_descriptor_table_t *fd_table = NULL;
void free_memory();


void mksfs(int fresh) {
    if (!inode_table) inode_table = malloc(sizeof(inode_table_t));
    if (!root_dir) root_dir = malloc(sizeof(directory_t));
    if (!fd_table) fd_table = malloc(sizeof(file_descriptor_table_t));
    signal(SIGINT, (__sighandler_t) free_memory);
    signal(SIGSTOP, (__sighandler_t) free_memory);
    signal(SIGTSTP, (__sighandler_t) free_memory);
    memset(inode_table, 0, sizeof(inode_table_t)); //Setting u memory space and initialzing memory structures. All constants taken from constants.h file
    memset(root_dir, 0, sizeof(directory_t));
    memset(fd_table, 0, sizeof(file_descriptor_table_t));
    inode_table_init(inode_table);
    directory_init(root_dir);
    fd_table_init(fd_table);
    switch (fresh) {
        case 1: { //if new file system required, re-initialize
            remove(MY_DISK);
            init_fresh_disk(MY_DISK, BLOCK_SIZE, BLOCK_COUNT);
            super_block_t super_block;
            super_block_init(&super_block);
            super_block.magic = 0xACBD0005; 
            super_block.block_size = BLOCK_SIZE;
            super_block.fs_size = BLOCK_COUNT;
            super_block.inode_table_length = (int) byteToBlock(sizeof(inode_table_t));
            super_block.root_dir_inode_index = 0;
            inode_table_setUsed(inode_table, 0); 
            inode_t *root_dir_inode = &inode_table->nodes[0];
            root_dir_inode->size = sizeof(directory_t);
            root_dir_inode->data_pointrs[0] = ROOT_DIR_DATABLOCK_INDEX;
            root_dir_inode->data_pointrs[1] = ROOT_DIR_DATABLOCK_INDEX + 1;
            root_dir_inode->data_pointrs[2] = ROOT_DIR_DATABLOCK_INDEX + 2;
            free_bitmap_t free_bitmap;
            free_bitmap_init(&free_bitmap);
            directory_init(root_dir); //After initiilizing everything, write the data to the disk
            write_blocks(SUPERBLOCK_DATABLOCK_INDEX, 1, &super_block);
            write_blocks(INODETABLE_DATABLOCK_INDEX, 8, inode_table);
            write_blocks(FREE_BITMAP_DATABLOCK_INDEX, 1, &free_bitmap);
            write_blocks(ROOT_DIR_DATABLOCK_INDEX, 3, root_dir);
            break;
        }
        case 0: {
            init_disk(MY_DISK, BLOCK_SIZE, BLOCK_COUNT);
            super_block_t super_block;
            read_from_disk(SUPERBLOCK_DATABLOCK_INDEX, BLOCK_SIZE, &super_block);
            read_from_disk(INODETABLE_DATABLOCK_INDEX, sizeof(inode_table_t), inode_table);
            read_from_disk(ROOT_DIR_DATABLOCK_INDEX, sizeof(directory_t), root_dir);
            break;
        }
        default:
            break;
    }
}


int sfs_getnextfilename(char *fname) { //The functions searches for the next non-free position in the directory table and increment the currentIndex of directory
    for (int i = root_dir->currentIndex; i < TOTAL_INODES - 1; ++i) {
        if (directory_isUsed(root_dir, i)) { 
            int lengthToRead = (int)
                    ((strlen(root_dir->entries[i].name) > MAXFILENAME) ?
                     MAXFILENAME :
                     strlen(root_dir->entries[i].name));
            memcpy(fname, root_dir->entries[i].name, (size_t) lengthToRead); 
            root_dir->currentIndex++; 
            return 1;
        }
    }
    root_dir->currentIndex = 0;
    return 0;
}


int sfs_getfilesize(const char *path) { //Gets the file size which is a stored variable in the respective I-node
    int inodeIndex = directory_getInodeIndex(root_dir, path);
    if (inodeIndex < 0) return -1;
    return inode_table->nodes[inodeIndex].size;
}


int sfs_fopen(char *name) {
    if (strlen(name) > MAXFILENAME) {
        return -1;
    }
    int inode_index = directory_getInodeIndex(root_dir, name);
    int FD_pos;  
    if (inode_index > 0) { //If file already has an entry in Inode: (i.e. file was created already)
        FD_pos = fd_table_contains_inode(fd_table, inode_index);
        if (FD_pos >= 0) return FD_pos; 
        FD_pos = fd_table_nextFree(fd_table);
        if (FD_pos < 0) return -1; //File descriptor is full, can't open any more files
        fd_table_setUsed(fd_table, FD_pos);
        fd_table->table[FD_pos].inode_index = inode_index;
        fd_table->table[FD_pos].inode = &inode_table->nodes[inode_index];
        fd_table->table[FD_pos].w_pointer = inode_table->nodes[inode_index].size; 
        fd_table->table[FD_pos].r_pointer = inode_table->nodes[inode_index].size;
    } else { //Creating the file if it hasn't been created yet:
        int dir_index = directory_nextFree(root_dir);
        if (dir_index < 0) return -1; //Directory is full, can't create any more files
        inode_index = inode_table_nextFree(inode_table);
        if (inode_index < 0) return -1; //I-Node is full, can't create any more I-nodes
        FD_pos = fd_table_nextFree(fd_table);
        if (FD_pos < 0) return -1; //File descriptor is full, can't open any more files
        directory_setUsed(root_dir, dir_index);
        inode_table_setUsed(inode_table, inode_index);
        fd_table_setUsed(fd_table, FD_pos);
        directory_entry_setName(&root_dir->entries[dir_index], name);
        directory_entry_setInodeIndex(&root_dir->entries[dir_index], inode_index);
        inode_table->nodes[inode_index].size = 0;
        fd_table->table[FD_pos].inode_index = inode_index;
        fd_table->table[FD_pos].inode = &inode_table->nodes[inode_index];
        fd_table->table[FD_pos].w_pointer = inode_table->nodes[inode_index].size;
        fd_table->table[FD_pos].r_pointer = inode_table->nodes[inode_index].size;
        write_blocks(INODETABLE_DATABLOCK_INDEX, (int) byteToBlock(sizeof(inode_table_t)), inode_table);
        write_blocks(ROOT_DIR_DATABLOCK_INDEX, (int) byteToBlock(sizeof(directory_t)), root_dir);
    }
    return FD_pos;
}


int sfs_fclose(int fileID) {
    if (fileID < 0) return -1; 
    if (fd_table_isFree(fd_table, fileID)) return -1; //The file was already closed
    fd_table_setFree(fd_table, fileID);
    fd_init(&fd_table->table[fileID]);
    return 0;
}


int sfs_fread(int fileID, char *buf, int length) {
    if (fileID < 0 || length < 0) return -1; 
    if (fd_table_isFree(fd_table, fileID)) return -1; //Can't read since the file descriptor table is full
    file_descriptor_t *fd = &fd_table->table[fileID];
    inode_t *inode = &inode_table->nodes[fd->inode_index];
    indirect_block_t indirect_block;
    if (inode->indirectPointer > 0) {
        read_from_disk(FILES_DATABLOCK_INDEX + inode->indirectPointer, BLOCK_SIZE, &indirect_block);
    };
    int bytes_left = (fd->r_pointer + length > inode->size) ?
                    inode->size - fd->r_pointer :
                    length;
    int bytes_already_read = 0;
    char *diskBuffer = malloc(BLOCK_SIZE);
    while (bytes_left > 0) {
        memset(diskBuffer, 0, BLOCK_SIZE); //Clearing the buffer used to read files
        int inode_index = fd->r_pointer / BLOCK_SIZE; //Getting the inode to access
        int data_start = fd->r_pointer % BLOCK_SIZE;
        int space_left_in_block = BLOCK_SIZE - data_start;
        int to_read = bytes_left > space_left_in_block ? space_left_in_block : bytes_left;  //If all data is in one data block, read that otherwise just acces the remaining data in theat block and then proceed to another one 
        int data_block_index; 
        if (inode_index < TOTAL_INODE_POINTERS) { 
            data_block_index = inode->data_pointrs[inode_index];
            if (data_block_index < 0) break;
        } else {
            int inirect_block_index = indirect_block_nextFree(&indirect_block);
            if (inirect_block_index < 0) { //Meaning no more space left on the data block
                return -1;
            } 
            data_block_index = indirect_block.pointers[inode_index - TOTAL_INODE_POINTERS];
            if (data_block_index < 0) break;
        }
        read_from_disk(FILES_DATABLOCK_INDEX + data_block_index, BLOCK_SIZE, diskBuffer); //After getting pointer of desired block, proceed to reading fromthe disk
        memcpy(buf, diskBuffer + data_start, (size_t) to_read); //Copying data to be read into a buffer
        fd->r_pointer += to_read;
        bytes_already_read += to_read;
        bytes_left -= to_read;
        buf += to_read;
    }
    free(diskBuffer);
    return bytes_already_read;
}


int sfs_fwrite(int fileID, const char *buf, int length) {
    if (fileID < 0 || length < 0) return -1; 
    int bytes_already_wrote = 0;
    int bytes_left = length;
    file_descriptor_t *fd = &fd_table->table[fileID];
    inode_t *inode = &inode_table->nodes[fd->inode_index];
    if (fd->w_pointer + length > ((TOTAL_DATABLOCKS / 4) * BLOCK_SIZE)) return -1; //The maximum file size you can have is (TOTAL_DATABLOCKS / 4) * BLOCK_SIZE) since 1 data block pointer is 4 bytes, you can store TOTAL_DATABLOCKS / 4 pointers in one block
    indirect_block_t indirect_block;
    indirect_block_init(&indirect_block);
    free_bitmap_t free_bitmap;
    read_from_disk(FREE_BITMAP_DATABLOCK_INDEX, sizeof(free_bitmap_t), &free_bitmap);
    char *diskBuffer = malloc(BLOCK_SIZE);
    while (bytes_left > 0) {
        memset(diskBuffer, 0, BLOCK_SIZE);  
        int inode_index = fd->w_pointer / BLOCK_SIZE;
        int data_start = fd->w_pointer % BLOCK_SIZE;
        int space_left_in_block = BLOCK_SIZE - data_start;
        /* If we can fit all we need to write in that data block, be it, if not, just fill the block */
        int to_write = bytes_left > space_left_in_block ? space_left_in_block : bytes_left; // Similar logic to the sfs_fread: If all data fits in one data block, write in one block , otherwise write onto first free one and then proceed to another one 
        int data_block_index;
        if (inode_index < TOTAL_INODE_POINTERS) {
            data_block_index = inode->data_pointrs[inode_index];
            if (data_block_index < 0) {
                data_block_index = free_bitmap_nextFree(&free_bitmap);
                if (data_block_index < 0) return -1; // Meaning Disk is full
                free_bitmap_setUsed(&free_bitmap, data_block_index);
                inode->data_pointrs[inode_index] = data_block_index;
            } else {
                read_from_disk(FILES_DATABLOCK_INDEX + data_block_index, BLOCK_SIZE, diskBuffer);
            }
        } else {
            if (inode->indirectPointer < 0) {
                int inirect_block_index = free_bitmap_nextFree(&free_bitmap);
                if (inirect_block_index < 0) return -1; //Meaning no more blocks availba
                free_bitmap_setUsed(&free_bitmap, inirect_block_index);
                inode->indirectPointer = inirect_block_index;
            } else {
                read_from_disk(FILES_DATABLOCK_INDEX + inode->indirectPointer, BLOCK_SIZE, &indirect_block);
            }
            data_block_index = indirect_block.pointers[inode_index - TOTAL_INODE_POINTERS];
            if (data_block_index < 0) {
                data_block_index = free_bitmap_nextFree(&free_bitmap);
                if (data_block_index < 0) return -1; //Meaning no more blocks available
                free_bitmap_setUsed(&free_bitmap, data_block_index);
                indirect_block.pointers[inode_index - TOTAL_INODE_POINTERS] = data_block_index;
                write_blocks(FILES_DATABLOCK_INDEX + inode->indirectPointer, 1, &indirect_block);
            } else {
                read_from_disk(FILES_DATABLOCK_INDEX + data_block_index, BLOCK_SIZE, diskBuffer);
            }
        }
        memcpy(diskBuffer + data_start, buf, (size_t) to_write);
        write_blocks(FILES_DATABLOCK_INDEX + data_block_index, 1, diskBuffer);
        fd->w_pointer += to_write;
        bytes_left -= to_write;
        bytes_already_wrote += to_write;
        buf += to_write;
        inode->size = fd->w_pointer > inode->size ? fd->w_pointer : inode->size; //If the write increased the filsze, it is recorded
    }
    free(diskBuffer);
    write_blocks(INODETABLE_DATABLOCK_INDEX, (int) byteToBlock(sizeof(inode_table_t)), inode_table);
    write_blocks(FREE_BITMAP_DATABLOCK_INDEX, (int) byteToBlock(sizeof(free_bitmap_t)), &free_bitmap);
    return bytes_already_wrote;
}

//These functions simply return the read or write pointers of the file descriptor
int sfs_frseek(int fileID, int loc) {
    if (fileID < 0 || loc < 0) return -1; 
    if (fd_table_isFree(fd_table, fileID)) return -1;
    fd_table->table[fileID].r_pointer = loc;
    return 0;
}


int sfs_fwseek(int fileID, int loc) {
    if (fileID < 0 || loc < 0) return -1; 
    if (fd_table_isFree(fd_table, fileID)) return -1; 
    fd_table->table[fileID].w_pointer = loc;
    return 0;
}


int sfs_remove(char *file) {
    int inodeIndex = directory_getInodeIndex(root_dir, file);
    if (inodeIndex < 0) return -1; // If no pointer to inode, then the file does not exist
    free_bitmap_t bitmap_free;
    read_from_disk(FREE_BITMAP_DATABLOCK_INDEX, BLOCK_SIZE, &bitmap_free);
    inode_t* inode = &inode_table->nodes[inodeIndex];
    int fdIndex = fd_table_contains_inode(fd_table, inodeIndex); //Remove pointer from the FD table and remove all data in data blocks from i-node pointers
    sfs_fclose(fdIndex);
    for (int i = 0; i < TOTAL_INODE_POINTERS; ++i) {
        if (inode->data_pointrs[i] > -1) {
            void* blank = malloc(BLOCK_SIZE);
            memset(blank, 0, BLOCK_SIZE);
            write_blocks(FILES_DATABLOCK_INDEX + inode->data_pointrs[i], 1, blank);
            free(blank);
            free_bitmap_setFree(&bitmap_free, inode->data_pointrs[i]);
        }
    }
    if (inode->indirectPointer > -1) {
        indirect_block_t indirect_block; //Deleting all data pointed to by an indirect block
        read_from_disk(FILES_DATABLOCK_INDEX + inode->indirectPointer, BLOCK_SIZE, &indirect_block);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); ++i) {
            if (indirect_block.pointers[i] > -1) {
                void* blank = malloc(BLOCK_SIZE);
                memset(blank, 0, BLOCK_SIZE);
                write_blocks(FILES_DATABLOCK_INDEX + indirect_block.pointers[i], 1, blank);
                free(blank);
                free_bitmap_setFree(&bitmap_free, indirect_block.pointers[i]);
            }
        }
        void* blank = malloc(BLOCK_SIZE); //Fill in the indirect block with emty space to delete any information left
        memset(blank, 0, BLOCK_SIZE);
        write_blocks(FILES_DATABLOCK_INDEX + inode->indirectPointer, 1, blank);
        free(blank);
        free_bitmap_setFree(&bitmap_free, inode->indirectPointer);
    }
    inode_init(inode);
    inode_table_setFree(inode_table, inodeIndex);
    int dirIndex = directory_getEntryIndex(root_dir, file);
    directory_entry_init(&root_dir->entries[dirIndex]);
    directory_setFree(root_dir, dirIndex);
    write_blocks(INODETABLE_DATABLOCK_INDEX, (int) byteToBlock(sizeof(inode_table_t)), inode_table); //Write onto disk all the changes done to the file system 
    write_blocks(FREE_BITMAP_DATABLOCK_INDEX, (int) byteToBlock(sizeof(free_bitmap_t)), &bitmap_free);
    write_blocks(ROOT_DIR_DATABLOCK_INDEX, (int) byteToBlock(sizeof(directory_t)), root_dir);
    return 0;
}

void free_memory() {
    free(inode_table);
    free(root_dir);
    free(fd_table);
    exit(0);
}

