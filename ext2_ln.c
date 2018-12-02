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
    if(argc < 4 || argc > 5){
      fprintf(stderr, "Usage: ext2_cp <image file name> [-s for symlink] <source path> <dest path> \n");
      exit(1); 
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    char *source_path;
    char *dest_path;
    source_path = argv[2];
    dest_path = argv[3];
    int symlink = 0;
    if(argc == 5){
        if(!(strcmp(argv[2],"-s") == 0)){
            fprintf(stderr, "Usage: ext2_cp <image file name> [-s for symlink] <source path> <dest path> \n");
            exit(1);
        }else{
            symlink = 1;    
            source_path = argv[3];
            dest_path = argv[4];
        }
    }

    //init global variables
    sb = (struct ext2_super_block *)(disk + 1024);
    gd  = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    block_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    inode_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    inodes   = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table );

    printf("%s\n",source_path );
    printf("%s\n",dest_path);

    //check path
    check_input_path(source_path,"ext2_ln");
    check_input_path(dest_path,"ext2_ln");

    //get parent dir
    char *source_path_parent_dir = get_parent_directory(source_path);
    char *source_path_child_name = get_child_name(source_path);
    printf("source_path_parent_dir :%s\n",source_path_parent_dir);
    printf("source_path_child_name :%s\n",source_path_child_name);


    //get child dir
    char *dest_path_parent_dir = get_parent_directory(dest_path);
    char *dest_path_child_name = get_child_name(dest_path);
    printf("dest_path_parent_dir :%s\n",dest_path_parent_dir);
    printf("dest_path_child_name :%s\n",dest_path_child_name);

    int source_path_parent_inode = find_inode(source_path_parent_dir,EXT2_S_IFDIR);
    if(source_path_parent_inode < 0){
        exit(ENOENT);
    }
    printf("source_path_parent_inode :%d\n",source_path_parent_inode);

    int dest_path_parent_inode = find_inode(dest_path_parent_dir,EXT2_S_IFDIR);
    if(dest_path_parent_inode < 0){
        exit(ENOENT);
    }
    printf("dest_path_parent_inode :%d\n",dest_path_parent_inode);

    //check if the link already existed
    struct ext2_dir_entry *dest_file_EEXIST_check = inode_find_dir(dest_path_child_name,dest_path_parent_inode,(symlink == 1)? EXT2_S_IFLNK:EXT2_S_IFREG);
    if(dest_file_EEXIST_check != NULL){
        fprintf(stderr, "Dest link already exist\n");
        exit(EEXIST);
    }
    
    //check if the hard link refers to a dir
    struct ext2_dir_entry *source_file_EISDIR_check = inode_find_dir(source_path_child_name,source_path_parent_inode,EXT2_S_IFDIR);
    if(source_file_EISDIR_check != NULL){
        fprintf(stderr, "Source file is a directory\n");
        exit(EISDIR);
    }

    //get source file dir_entry
    struct ext2_dir_entry *source_file_dir = inode_find_dir(source_path_child_name,source_path_parent_inode,EXT2_S_IFREG);
    if(source_file_dir == NULL){
        fprintf(stderr, "Source file does not exist\n");
        exit(ENOENT);
    }
    if(symlink){
        int soft_inode = add_child(dest_path_parent_inode,dest_path_child_name,EXT2_S_IFLNK,-1);
        int bytes_needed = strlen(source_path);

        int blocks_needed = strlen(source_path);
        if((blocks_needed % EXT2_BLOCK_SIZE) == 0){
            blocks_needed = blocks_needed / EXT2_BLOCK_SIZE;
        }else{
            blocks_needed = (blocks_needed / EXT2_BLOCK_SIZE) + 1;    
        }
        
        if(blocks_needed > gd->bg_free_blocks_count){
            fprintf(stderr, "Not enough free blocks to allocate the path\n");
            exit(EFBIG);
        }

        for (int i=0;i<12;i++){
            if(blocks_needed == 0){
                return 0;
            }
            inodes[soft_inode].i_block[i] = get_new_block() + 1;
            inodes[soft_inode].i_blocks += 2;
            if(bytes_needed < EXT2_BLOCK_SIZE){
                memcpy(disk + inodes[soft_inode].i_block[i] * EXT2_BLOCK_SIZE, source_path, bytes_needed);
                inodes[soft_inode].i_size += bytes_needed;
                bytes_needed = 0;
                
            }else{
                memcpy(disk + inodes[soft_inode].i_block[i] * EXT2_BLOCK_SIZE, source_path, EXT2_BLOCK_SIZE);
                bytes_needed -= EXT2_BLOCK_SIZE;
                inodes[soft_inode].i_size += EXT2_BLOCK_SIZE;
                
            }
            blocks_needed -= 1;
        }
        if(blocks_needed > 0){
            inodes[soft_inode].i_block[12] = get_new_block() + 1;
            inodes[soft_inode].i_blocks += 2;
            int *indirect_block = (int *)(disk +  EXT2_BLOCK_SIZE * (inodes[soft_inode].i_block)[12]);
            for(int i = 0; i < 256; i++) {
                if(blocks_needed == 0) {
                    return 0;
                }
                inodes[soft_inode].i_blocks += 2;
                indirect_block[i] = get_new_block() + 1;
                if(bytes_needed < EXT2_BLOCK_SIZE){
                    memcpy(disk + inodes[soft_inode].i_block[i] * EXT2_BLOCK_SIZE, source_path, bytes_needed);
                    inodes[soft_inode].i_size += bytes_needed;
                    bytes_needed = 0;
                }else{
                    memcpy(disk + inodes[soft_inode].i_block[i] * EXT2_BLOCK_SIZE, source_path, EXT2_BLOCK_SIZE);
                    bytes_needed -= EXT2_BLOCK_SIZE;
                    inodes[soft_inode].i_size += EXT2_BLOCK_SIZE;
                }
                blocks_needed -= 1;
            }
        }
    }else{
        int hardlink_inode = add_child(dest_path_parent_inode,dest_path_child_name,EXT2_S_IFREG,source_file_dir->inode - 1);
        printf("source_file_inode :%d\n",hardlink_inode);
    }
}   