#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
/* TODO: Phase 1 */
//Data structures to be used:

struct __attribute__ ((packed)) superBlock{
	uint32_t signature[2];
	uint16_t disk_size;
	uint16_t root_index;
	uint16_t data_index;
	uint16_t num_blocks;
	uint8_t FAT_size;
	uint8_t padding[4079];
};
//?

struct __attribute__ ((packed)) FS_FAT{
	int num_entries;
	//[block_index][entry index]
	uint16_t** entries;
};

struct __attribute__ ((packed)) root_entry{
	uint8_t file_name[FS_FILENAME_LEN];
	uint32_t file_size;
	uint16_t first_block;
	uint8_t padding[10];
};

struct __attribute__((__packed__)) File_Desc {
	uint8_t file_name[FS_FILENAME_LEN];
	uint32_t* file_size;
	int32_t fd;
	uint32_t offset;
};


//Globals
static struct superBlock* SB;
static struct FS_FAT* FAT;
static struct root_entry ROOT_DIR[FS_FILE_MAX_COUNT];

struct File_Desc openFiles[FS_OPEN_MAX_COUNT];  // FIXME - SHOULD THIS BE STATIC?
unsigned int numFilesOpen = 0;
unsigned int currentFD = 0;
uint16_t extend_write(uint8_t*);
uint16_t data_block_index(uint8_t*, int);
uint16_t next_block(uint16_t);
int fs_mount(const char *diskname)
{
	int i;
	/* TODO: Phase 1 */
	if(block_disk_open(diskname)){
		return -1;
	}
	//create super block
	void* buffer = malloc(BLOCK_SIZE);
	SB = malloc(sizeof(struct superBlock));
	block_read(0, buffer);
	SB = buffer;

	if(memcmp(SB->signature, "ECS150FS", 8)){
		return -1;
	}
	if(SB->disk_size !=  block_disk_count()){
		return -1;
	}

	//create FAT
	
	FAT = malloc(sizeof(struct FS_FAT));
	FAT->num_entries = SB->num_blocks; 
	FAT->entries = malloc(SB->FAT_size * sizeof(uint16_t*));
	for(i = 0; i < SB->FAT_size; i++){
		FAT->entries[i] = malloc((BLOCK_SIZE/2) * sizeof(uint16_t));
	}
	
	for(i = 1; i <= SB->FAT_size; i++)
		block_read(i, (void*)(FAT->entries[i-1]));
		
	//create root directory
	block_read(SB->root_index, (void*)ROOT_DIR);

	//ensures openFiles array has FD's that wont accidentally be accessed since they are 0
	for(i = 0; i < FS_OPEN_MAX_COUNT; i++){
		openFiles[i].fd = -1;
	}	
	return 0;
}

int fs_umount(void)
{
	
	//write back to virtual disk
	int i;
	block_write(0, SB);
	//probably need to change this when i figure out whats wrong with FAT mounting
	for(i = 1; i <= SB->FAT_size; i++){
		block_write(i, (void*)(FAT->entries[i-1]));
	}
	

	//This is considered a bad address to write?
	block_write(SB->root_index, (void*)ROOT_DIR);
	// write back data blocks
	/*
	for(i = 0; i < SB->num_blocks; i++)
		block_write(SB->data_index[i],
	*/

	free(SB);
	free(FAT);
	return 0;
	
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	printf("FS Info:\n");
	printf("total_blk_count=%d\n", SB->disk_size);
	printf("fat_blk_count=%d\n", SB->FAT_size);
	printf("rdir_blk=%d\n", SB->root_index);
	printf("data_blk=%d\n", SB->data_index);
	printf("data_blk_count=%d\n", SB->num_blocks);
	int free = 0;
	int i;
	for(int j = 0; j < SB->FAT_size; j++){
		for(i = 0; i < FAT->num_entries; i++){
			if(FAT->entries[j][i] == 0)
				free++;
		}
	}
	printf("fat_free_ratio=%d/%d\n", free, FAT->num_entries);
	free = 0;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(*ROOT_DIR[i].file_name == '\0')
			free++;
	}
	printf("rdir_free_ratio=%d/%d\n", free, FS_FILE_MAX_COUNT);

	return 0;
}

int fs_create(const char *filename)
{	
	int i;
	if(sizeof(filename) > FS_FILENAME_LEN)
		return -1;
	/*should check last character of string is null terminiated, this doesnt work	
	if(filename[strlen(filename) + 1] != '\0')
		return -1;
	*/
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(!strcmp((char*)ROOT_DIR[i].file_name, filename))
			return -1;
	}
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(*ROOT_DIR[i].file_name == '\0')
			break;
	}
	if(i == FS_FILE_MAX_COUNT)
                return -1;
	memcpy(ROOT_DIR[i].file_name, filename, FS_FILENAME_LEN);
	ROOT_DIR[i].file_size = 0;
	ROOT_DIR[i].first_block = FAT_EOC;

	return 0;

	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	int i;
	if(sizeof(filename) > FS_FILENAME_LEN)
		return -1;

	/*same problem as create:
	if(filename[sizeof(filename) - 1] != '\0')
		return -1;
	*/
	//check if file is open? if so fail/
	//

	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(!strcmp((char*)ROOT_DIR[i].file_name, filename))
			break;
	}
	if(i == FS_FILE_MAX_COUNT)
		return -1;
	memset(ROOT_DIR[i].file_name, 0, FS_FILENAME_LEN);
	ROOT_DIR[i].file_size = 0;
	//delete chain from fat
	uint16_t current = ROOT_DIR[i].first_block;
	int first,second;
	while (current != FAT_EOC){
		///get the exact index of current chain in FAT
		first = current / (BLOCK_SIZE/2);
		second = current % (BLOCK_SIZE/2);
		current = FAT->entries[first][second];
		//need to free data block associated with this fat entry as well
		FAT->entries[first][second] = 0;
	}

	ROOT_DIR[i].first_block = 0;
	return 0;

}

int fs_ls(void)
{	
	if(SB == NULL)
		return -1;	
	int i;
	printf("FS Ls:\n");
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(*ROOT_DIR[i].file_name == '\0')
			continue;
		else{
			struct root_entry current = ROOT_DIR[i];
			printf("file: %s, size: %d, data_blk: %d\n",
				(char*)current.file_name, current.file_size, current.first_block);
		}
	}
	return 0;
}

int fs_open(const char *filename)
{
    if (filename == NULL)
        return -1;

    int root;
    int rootFound = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (!strcmp((char*)ROOT_DIR[i].file_name, filename)) {  // will char* cast work?
            root = i;
            rootFound = 1;
            break;
        }
    }
    if (!rootFound || numFilesOpen >= FS_OPEN_MAX_COUNT)
        return -1;

    struct File_Desc file;
    // Initalize file_descriptor
    memcpy((void*)file.file_name, (void*)filename, FS_FILENAME_LEN);
    file.file_size = &ROOT_DIR[root].file_size;
    file.fd = currentFD;
    file.offset = 0;
    openFiles[numFilesOpen] = file;
    // Update open globals
    numFilesOpen++;
    currentFD++;
    return file.fd;
    /* TODO: Phase 3 */
}

int fs_close(int fd)
{
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT)
        return -1;
    unsigned int i;
    unsigned int index;
    unsigned int indexFound = 0;
    for (i = 0; i < numFilesOpen; i++) {
        if (openFiles[i].fd == fd) {
            index = i;
            indexFound = 1;
            break;
        }
    }
    if (!indexFound)
        return -1;

    // Move all open files by 1 instead of equating elements to NULL - to avoid nullptr exceptions & reaches
    for (i = index; i < (numFilesOpen); i++) {
        openFiles[i] = openFiles[i + 1];  		// FIXME - check for overflow errors
    }
    numFilesOpen--;

    return 0;
    /* TODO: Phase 3 */
}

int fs_stat(int fd)
{
    if (fd < 0)
        return -1;
	
    unsigned int i;
    unsigned int index = 0;
    unsigned int indexFound = 0;
    for (i = 0; i < numFilesOpen; i++) {
        if (openFiles[i].fd == fd) {
            index = i;
            indexFound = 1;
            break;
        }
    }

    if (!indexFound) {
        return -1;
    } else {
        return *openFiles[index].file_size;
    }
    /* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
    if (fd < 0)
        return -1;
    unsigned int i;
    unsigned int index = 0;
    unsigned int indexFound = 0;
    for (i = 0; i < numFilesOpen; i++) {
        if (openFiles[i].fd == fd) {
            index = i;
            indexFound = 1;
            break;
        }
    }
    if (!indexFound || offset > *openFiles[index].file_size)
        return -1;

    openFiles[index].offset = offset;

    return 0;
    /* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	struct root_entry* cur_file;
	struct File_Desc file2write;
        void* bounce[BLOCK_SIZE];
        unsigned int i, bytes_wrote = 0, op_size;
	uint16_t block;
        for(i = 0; i < numFilesOpen; i++){
                if(openFiles[i].fd == fd){
                        file2write = openFiles[i];
                        break;
                }
        }
        for (i = 0; i < FS_FILE_MAX_COUNT; i++) {
            if (!strcmp((char*)ROOT_DIR[i].file_name, (char*)file2write.file_name)) {
                cur_file = &ROOT_DIR[i];
                break;
        }
    }
        //request fd not found
        if(i == numFilesOpen  || fd <0 )
                return -1;
	
	while(bytes_wrote != count){
		block = data_block_index(file2write.file_name, file2write.offset);
		if(block == FAT_EOC)
			block = extend_write(file2write.file_name);
		
		if(block == 0)
			return bytes_wrote;//ran out of blocks to write

		//not at start of block
		if((file2write.offset % BLOCK_SIZE) != 0){
			if (count < (BLOCK_SIZE - (file2write.offset % BLOCK_SIZE)))
					op_size = count;
			else
				op_size = (BLOCK_SIZE - (file2write.offset % BLOCK_SIZE));
			
			block_read((SB->data_index + block), bounce);
			memcpy(bounce[file2write.offset % BLOCK_SIZE], buf + bytes_wrote, op_size);
			block_write((SB->data_index + block), bounce);
			bytes_wrote += op_size;
			file2write.offset += op_size;
			continue;
		}
		else if((count-bytes_wrote) < BLOCK_SIZE){
			
			op_size = count-bytes_wrote;
			
			block_read((SB->data_index + block), bounce);
                        memcpy(bounce, buf + bytes_wrote,op_size);
                        block_write((SB->data_index + block), bounce);
			bytes_wrote += op_size;
                        file2write.offset += op_size;
			break;
		}
		else{
			op_size = BLOCK_SIZE;
			memcpy(bounce, buf + bytes_wrote, op_size);
			block_write((SB->data_index + block), bounce);
			bytes_wrote += op_size;
			file2write.offset += op_size;
			continue;
		}

	}
	cur_file->file_size = file2write.offset;
	return  bytes_wrote;

}

int fs_read(int fd, void *buf, size_t count)
{	
	struct File_Desc file2read;
	void* bounce[BLOCK_SIZE];
	unsigned int i, bytes_read = 0, op_size, end = 0, orig_off;

	uint16_t block;
	for(i = 0; i < numFilesOpen; i++){
		if(openFiles[i].fd == fd){
			file2read = openFiles[i];	
			break;
		}
	}
	//request fd not found
	if(i == numFilesOpen  || fd <0 )
		return -1;
	if(count > (*file2read.file_size - file2read.offset))
			return -1;//read will go out of bounds
	orig_off = file2read.offset;
	//do the read
	while(bytes_read != count){
		block = data_block_index(file2read.file_name, file2read.offset);
		if(block == FAT_EOC)
			break;
		//case 1, offset not at beginning of a block
		if((file2read.offset % BLOCK_SIZE) != 0){
			block_read((SB->data_index + block), bounce);
			if((*file2read.file_size - file2read.offset) < ((count+orig_off) - file2read.offset)){//reaching end of file
				op_size = *file2read.file_size - file2read.offset;
				end = 1;
			}
			else if((BLOCK_SIZE - (file2read.offset % BLOCK_SIZE)) > count)
				op_size = count; // small operation
			else
				op_size = BLOCK_SIZE - (file2read.offset % BLOCK_SIZE);
			
			memcpy(buf + bytes_read, bounce[file2read.offset % BLOCK_SIZE], op_size);
			bytes_read += op_size;
			file2read.offset += op_size;
			if(end)
				break;
			continue; 
		}
		//case 2, reading from beginning but not whole block
		else if((count-bytes_read) < BLOCK_SIZE){
			if((*file2read.file_size - file2read.offset) < ((count+orig_off)-file2read.offset))
                                op_size = *file2read.file_size - file2read.offset;//reaching end of file
			else
				op_size = count - bytes_read;
			block_read((SB->data_index + block), bounce);
			memcpy(buf + bytes_read, bounce, op_size);
			bytes_read += op_size;
			file2read.offset += op_size;
			break;
		}
		//case 3, reading whole block from beginning
		else{
                        block_read((SB->data_index + block), buf + bytes_read);
                        op_size = BLOCK_SIZE;
                        bytes_read += op_size;
                        file2read.offset += op_size;
                        continue;
                }

	}
	return bytes_read;
}

uint16_t next_block(uint16_t current){
	int blk, off;
	blk = current / (BLOCK_SIZE/2);
	off = current % (BLOCK_SIZE/2);
	return FAT->entries[blk][off];
}

uint16_t data_block_index(uint8_t* filename, int offset){
	int blocks_deep, i;
	uint16_t block;
	blocks_deep = offset / BLOCK_SIZE;
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
                if(!strcmp((char*)ROOT_DIR[i].file_name, (char*)filename)){
                        block = ROOT_DIR[i].first_block;
			break;
		}
        }
	while(blocks_deep){
		block = next_block(block);
		blocks_deep--;
	}
	return block;

}

uint16_t extend_write(uint8_t* filename){
	int k, i, j, blk, off, first = 0;
	uint16_t block, new_block = 0;
	for(k = 0; k < FS_FILE_MAX_COUNT; k++){
                if(!strcmp((char*)ROOT_DIR[k].file_name, (char*)filename)){
                        block = ROOT_DIR[k].first_block;
                        break;
		}
        }
	
	if(block == FAT_EOC)
		first = 1;
	for(j = 0; j < SB->FAT_size; j++){
                for(i = 0; i < 2048; i++){
                        if(FAT->entries[j][i] == 0){
                                FAT->entries[j][i] = FAT_EOC;
				new_block = (j*2048) + i;
				break;
			}
			continue;
                }
		break; ///this is going to break out prematurely, how to do this ?
        }
	if(first)
		ROOT_DIR[k].first_block = new_block;
	else{
		while(block != FAT_EOC){
                	blk = block / (BLOCK_SIZE/2);
                	off = block % (BLOCK_SIZE/2);
                	block = FAT->entries[blk][off];
        	}
		FAT->entries[blk][off] = new_block;
	}
	if (new_block == 0)
		return -0;
	return new_block;

}
