#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "helpers.h"

unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_group_desc *gd; 
unsigned char* block_bitmap;
unsigned char* inode_bitmap;
struct ext2_inode *inodes; 

int free_inodes_checker();
int free_blocks_checker();
int inode_checker();
int check_block_bitmap_reverse(int block);
int check_inode_bitmap_reverse(int inode);


int main(int argc, char *argv[]){
    if(argc != 2){
      fprintf(stderr, "Usage: ext2_mkdir <image file name>\n");
      exit(1); 
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    sb = (struct ext2_super_block *)(disk + 1024);
    gd  = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    block_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    inode_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    inodes   = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table );

    int total_counter = 0;

    // checker 1
    int free_inodes_counter = free_inodes_checker();
    printf("free_inodes_counter: %d\n",free_inodes_counter);
    int free_blocks_counter = free_blocks_checker();
    printf("free_blocks_counter: %d\n",free_blocks_counter);

    if(sb->s_free_inodes_count != free_inodes_counter){
        int difference = abs(free_inodes_counter -(int)sb->s_free_inodes_count);
        sb->s_free_inodes_count = free_inodes_counter;
        printf("Fixed: %s's %s counter was off by %d compared to the bitmap\n","superblock","free inodes",difference);
        total_counter += 1;
    }
    if(sb->s_free_blocks_count != free_blocks_counter){
        int difference = abs(free_blocks_counter - (int)sb->s_free_blocks_count);
        sb->s_free_blocks_count = free_blocks_counter;
        printf("Fixed: %s's %s counter was off by %d compared to the bitmap\n","superblock","free blocks",difference);
        total_counter += 1;
    }

    if(gd->bg_free_inodes_count != free_inodes_counter){
        int difference = abs(free_inodes_counter - (int)gd->bg_free_inodes_count);
        gd->bg_free_inodes_count = free_inodes_counter;
        printf("Fixed: %s's %s counter was off by %d compared to the bitmap\n","block group","free inodes",difference);
        total_counter += 1;
    }
    if(gd->bg_free_blocks_count != free_blocks_counter){
        int difference = abs(free_blocks_counter - (int)gd->bg_free_blocks_count);
        gd->bg_free_blocks_count = free_blocks_counter;
        printf("Fixed: %s's %s counter was off by %d compared to the bitmap\n","block group","free blocks",difference);
        total_counter += 1;
    }

    total_counter += inode_checker();

}

int inode_checker(){
    int counter = 0;
    for (int i=0;i<sb->s_inodes_count;i++){
        if ((i > (EXT2_GOOD_OLD_FIRST_INO - 1)|| i == (EXT2_ROOT_INO - 1)) && (inodes[i].i_blocks != 0) && (((inodes[i].i_mode & 0xF000) == EXT2_S_IFDIR) || ((inodes[i].i_mode & 0xF000) == EXT2_S_IFREG) || ((inodes[i].i_mode & 0xF000) == EXT2_S_IFLNK))) {
            if(!(check_inode_bitmap_reverse(i))){
                restore_inode_bitmap(i);
                counter +=1;
                printf("Fixed: inode [%d] not marked as in-use\n",i+1);
            }
            if(!(inodes[i].i_dtime == 0)){
                inodes[i].i_dtime = 0;
                counter +=1;
                printf("Fixed: valid inode marked for deletion: [%d]\n",i+1);
            }
            int inconsistent_blocks = 0;
            for(int j = 0; j < (inodes[i].i_blocks / 2); j++) {
                int rec = 0;
                if(!(check_block_bitmap_reverse(inodes[i].i_block[j] - 1))){
                    restore_block_bitmap(inodes[i].i_block[j] - 1);
                    counter += 1;
                    inconsistent_blocks += 1;
                }
                while(rec < EXT2_BLOCK_SIZE){
                    struct ext2_dir_entry *entry = (struct ext2_dir_entry*) (disk + 1024* inodes[i].i_block[j] + rec);
                    if(!(inode_dir_type_compare(inodes[i].i_mode,entry->file_type))){
                        entry->file_type = inode_dir_type_switch(inodes[i].i_mode);
                        counter += 1;
                        printf("Fixed: Entry type vs inode mismatch: inode [%d]\n",i+1);
                    }
                    rec += entry->rec_len;
                }
            }
            if(inconsistent_blocks){
                printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", inconsistent_blocks,i+1);
            }
        }
    }
    return 0;
}

int free_inodes_checker(){
    int free_inodes_counter = 0;
    for (int byte = 0; byte < 4; byte++){
        for (int bit = 0; bit < 8; bit++){
            int in_use = inode_bitmap[byte] >> bit & 1;
            if(!in_use){
              free_inodes_counter += 1;
            }
        }
    }
    return free_inodes_counter;


}

int free_blocks_checker(){
    int free_blocks_counter = 0;
    for (int byte = 0; byte < 16; byte++){
        for (int bit = 0; bit < 8; bit++){
            int in_use = block_bitmap[byte] >> bit & 1;
            if(!in_use){
              free_blocks_counter += 1;
            }
        }
    }
    return free_blocks_counter;
}

int check_block_bitmap_reverse(int block){
  if(!(block_bitmap[block / 8] >> (block % 8) & 1)){
    return 0;
  }
  return 1;
}

int check_inode_bitmap_reverse(int inode){
  if(!(inode_bitmap[inode / 8] >> (inode % 8) & 1)){
    return 0;
  }
  return 1;
}