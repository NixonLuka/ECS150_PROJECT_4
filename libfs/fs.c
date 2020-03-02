#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

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

};

//globals
static struct superBlock* SB;
static struct FS_FAT* FAT;
static struct root_entry ROOT_DIR[FS_FILE_MAX_COUNT];

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

	//create FAT
	
	FAT = malloc(sizeof(struct FS_FAT));
	FAT->num_entries = (BLOCK_SIZE/2) * SB->FAT_size; 
	FAT->entries = malloc(SB->FAT_size * sizeof(uint16_t*));
	for(i = 0; i < SB->FAT_size; i++){
		FAT->entries[i] = malloc((BLOCK_SIZE/2) * sizeof(uint16_t));
	}
	
	for(i = 1; i <= SB->FAT_size; i++)
		block_read(i, (void*)(FAT->entries[i-1]));
		
	//create root directory
	
	block_read(SB->root_index, (void*)ROOT_DIR);

	
	return 0;
}

int fs_umount(void)
{
	//write back to virtual disk
	
	/* TODO: Phase 1 */
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
	for(i = 0; i < FAT->num_entries; i++){
		if(FAT->entries[i] == 0)
			free++;
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
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

