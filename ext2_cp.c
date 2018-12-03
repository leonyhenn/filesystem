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

void copy_file(int dest_file_child_inode, FILE *source_file,int blocks_needed);

int main(int argc, char *argv[]){
    if(argc != 4){
      fprintf(stderr, "Usage: ext2_cp <image file name> <path to source file> <path to dest>\n");
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

    char *path_to_source_file = argv[2];
    char *path_to_dest = argv[3];

    check_input_path(path_to_dest,"ext2_cp");

    //source file handling
    FILE *source_file;
    if((source_file = fopen(argv[2], "rb")) == NULL) {
        fprintf(stderr, "Can't open source file %s\n", path_to_source_file);
        exit(ENOENT);
    }
    fseek(source_file, 0L, SEEK_END);
    int blocks_needed = ftell(source_file);
    fseek(source_file, 0L, SEEK_SET);
    
    if((blocks_needed % EXT2_BLOCK_SIZE) == 0){
        blocks_needed = blocks_needed / EXT2_BLOCK_SIZE;
    }else{
        blocks_needed = (blocks_needed / EXT2_BLOCK_SIZE) + 1;    
    }
    
    if(blocks_needed > gd->bg_free_blocks_count){
        fprintf(stderr, "Not enough free blocks to allocate the file\n");
        fclose(source_file);
        exit(EFBIG);
    }

    //dest handling
    char *dest_file_parent_dir = get_parent_directory(path_to_dest);
    char *dest_file_child_name = get_child_name(path_to_dest);
    

    int dest_file_parent_inode = find_inode(dest_file_parent_dir,EXT2_S_IFDIR);
    if(dest_file_parent_inode < 0){
        exit(ENOENT);
    }
    

    struct ext2_dir_entry* dest_file_child_dir = inode_find_dir(dest_file_child_name,dest_file_parent_inode,EXT2_S_IFREG);
    if(dest_file_child_dir != NULL){
        fprintf(stderr,"Dest file already exist");
        exit(EEXIST);
    }

    int dest_file_child_inode = add_child(dest_file_parent_inode, dest_file_child_name, EXT2_S_IFREG, -1);
    

    copy_file(dest_file_child_inode, source_file,blocks_needed);

    return 0;
    
}
void copy_file(int dest_file_child_inode, FILE *source_file,int blocks_needed){
    for (int i=0;i<12;i++){
        if(blocks_needed == 0){
            return;
        }
        inodes[dest_file_child_inode].i_block[i] = get_new_block() + 1;
        inodes[dest_file_child_inode].i_blocks += 2;
        int bytes_read = fread (disk + inodes[dest_file_child_inode].i_block[i] * EXT2_BLOCK_SIZE,1,EXT2_BLOCK_SIZE,source_file);
        inodes[dest_file_child_inode].i_size += bytes_read;
        blocks_needed -= 1;

    }

    if(blocks_needed > 0){
        inodes[dest_file_child_inode].i_block[12] = get_new_block() + 1;
        inodes[dest_file_child_inode].i_blocks += 2;
        int *indirect_block = (int *)(disk +  EXT2_BLOCK_SIZE * (inodes[dest_file_child_inode].i_block)[12]);
        for(int i = 0; i < 256; i++) {
            if(blocks_needed == 0) {
                return;
            }
            indirect_block[i] = get_new_block() + 1;
            inodes[dest_file_child_inode].i_blocks += 2;
            int bytes_read = fread(disk + indirect_block[i] * EXT2_BLOCK_SIZE, 1, EXT2_BLOCK_SIZE, source_file);
            inodes[dest_file_child_inode].i_size += bytes_read;
            blocks_needed -= 1;
        }
    }
    return;
}

