#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "helpers.h"
#include <time.h>

unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_group_desc *gd; 
unsigned char* block_bitmap;
unsigned char* inode_bitmap;
struct ext2_inode *inodes; 

void remove_file(int parent_inode,int victim_inode);
void free_inode(int inode);

int main(int argc, char *argv[]){
    if(argc != 3){
      fprintf(stderr, "Usage: ext2_mkdir <image file name> <path to link>\n");
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


    char *path_to_victim = argv[2];

    check_input_path(path_to_victim,"ext2_rm");

    char *victim_file_parent_dir = get_parent_directory(path_to_victim);
    char *victim_file_child_name = get_child_name(path_to_victim);
    

    int victim_file_parent_inode = find_inode(victim_file_parent_dir,EXT2_S_IFDIR);
    if(victim_file_parent_inode < 0){
        exit(ENOENT);
    }
    

    struct ext2_dir_entry* victim_file_child_dir = inode_find_dir(victim_file_child_name,victim_file_parent_inode,EXT2_S_IFREG);
    if(victim_file_child_dir == NULL){
        fprintf(stderr,"Victim file DNE.");
        exit(ENOENT);
    }
    
    
    if(victim_file_child_dir->file_type == EXT2_FT_DIR){
        fprintf(stderr, "Victim file is a directory\n");
        exit(EISDIR);
    }
    free_inode(victim_file_child_dir->inode - 1);
    remove_file(victim_file_parent_inode, victim_file_child_dir->inode);


}
void remove_file(int parent_inode,int victim_inode){
    inodes[victim_inode].i_links_count -= 1;
    for(int j = 0; j < (inodes[parent_inode].i_blocks / 2); j++) {
        int rec = 0;
        struct ext2_dir_entry *prev = NULL;
        while(rec < EXT2_BLOCK_SIZE){
            struct ext2_dir_entry *entry = (struct ext2_dir_entry*) (disk + 1024* inodes[parent_inode].i_block[j] + rec);
            if(entry->inode == victim_inode){
                if(prev == NULL){
                    
                    entry->inode = 0;
                }else{
                    prev->rec_len += entry->rec_len;
                }
            }
            rec += entry->rec_len;
            prev = entry;
        }

      
    }
}
void free_block_bitmap(int block){
  if(!(block_bitmap[block / 8] >> (block % 8) & 1)){
    fprintf(stderr, "%d: this block is not in use.\n",block );
    exit(EINVAL);
  }
  block_bitmap[block / 8] &=  (~(1<<(block % 8))); 

  sb->s_free_blocks_count++; 
  gd->bg_free_blocks_count++;
}

void free_inode_bitmap(int inode){
  if(!(inode_bitmap[inode / 8] >> (inode % 8) & 1)){
    fprintf(stderr, "this inode is not in use.\n" );
    exit(EINVAL);
  }
  inode_bitmap[inode / 8] &=  (~(1<<(inode % 8))); 

  sb->s_free_inodes_count++; 
  gd->bg_free_inodes_count++;


}
void free_inode(int inode){
    inodes[inode].i_dtime = (int)time(NULL);
    free_inode_bitmap(inode);
    for(int i = 0; i < 12; ++i) {
        if(inodes[inode].i_block[i] != 0){
            free_block_bitmap(inodes[inode].i_block[i] - 1);
        }else{
            return;
        }
    }
    if(inodes[inode].i_block[12] != 0){
        int *indirect_block = (int *)(disk +  EXT2_BLOCK_SIZE * (inodes[inode].i_block)[12]);
        for(int i = 0; i < 256; i++) {
            if(indirect_block[i] == 0) {
                free_block_bitmap(inodes[inode].i_block[12] - 1);
                return;
            }
            free_block_bitmap(indirect_block[i] - 1);
        }
        
    }
    

}