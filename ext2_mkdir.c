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


int main(int argc, char *argv[]){
    if(argc != 3){
      fprintf(stderr, "Usage: ext2_mkdir <image file name> <path>\n");
      exit(1); 
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    char *expected_path = argv[2];

    //init global variables
    sb = (struct ext2_super_block *)(disk + 1024);
    gd  = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    block_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    inode_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    inodes   = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table );
    
    //check path
    check_input_path(expected_path,"ext2_mkdir");

    //get parent dir
    char *parent_dir = get_parent_directory(expected_path);
    char *child_name = get_child_name(expected_path);

    //recursively get parent inode, return error if any part is missing
    int parent_inode = find_inode(parent_dir,EXT2_S_IFDIR);
    if(parent_inode < 0){
        exit(ENOENT);
    }
    
    int child_inode = add_child(parent_inode,child_name,EXT2_S_IFDIR,-1);
    
    gd->bg_used_dirs_count++; 
    add_child(child_inode,".",EXT2_S_IFDIR,child_inode);
    add_child(child_inode,"..",EXT2_S_IFDIR,parent_inode);
    return 0;
}