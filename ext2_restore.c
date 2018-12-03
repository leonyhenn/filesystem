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

void go_through_file(int parent_inode,char *victim_file_child_name);

int main(int argc, char *argv[]){
    if(argc != 3){
      fprintf(stderr, "Usage: ext2_mkdir <image file name> <path to file>\n");
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
    printf("ext2_rm.c victim_file_parent_dir: %s\n",victim_file_parent_dir);
    printf("ext2_rm.c victim_file_child_name: %s\n",victim_file_child_name);

    int victim_file_parent_inode = find_inode(victim_file_parent_dir,EXT2_S_IFDIR);
    if(victim_file_parent_inode < 0){
        exit(ENOENT);
    }
    printf("ext2_rm.c victim_file_parent_inode: %d\n",victim_file_parent_inode);

    struct ext2_dir_entry* victim_file_child_dir = inode_find_dir(victim_file_child_name,victim_file_parent_inode,EXT2_S_IFREG);

    if(victim_file_child_dir != NULL){
        fprintf(stderr,"Victim file still exist.");
        exit(ENOENT);
    }
    
    
    
    go_through_file(victim_file_parent_inode,victim_file_child_name);


}

void go_through_file(int parent_inode,char *victim_file_child_name){
    for(int j = 0; j < (inodes[parent_inode].i_blocks / 2); j++) {
        int rec = 0;
        struct ext2_dir_entry *prev = NULL;
        while(rec < EXT2_BLOCK_SIZE){
            struct ext2_dir_entry *entry = (struct ext2_dir_entry*) (disk + 1024* inodes[parent_inode].i_block[j] + rec);
            //find gap between files, gap should be able to fit both files
            if(entry->rec_len >= (((sizeof(struct ext2_dir_entry)+entry->name_len) + 4 - ((sizeof(struct ext2_dir_entry)+entry->name_len) % 4))) + (((sizeof(struct ext2_dir_entry)+strlen(victim_file_child_name)) + 4 - ((sizeof(struct ext2_dir_entry)+strlen(victim_file_child_name)) % 4)))){
                printf("new\n");
                int rest;
                while(rest < (entry->rec_len - ((sizeof(struct ext2_dir_entry)+entry->name_len) + 4 - ((sizeof(struct ext2_dir_entry)+entry->name_len) % 4)))){
                    // struct ext2_dir_entry *possible = (struct ext2_dir_entry *)(disk + 1024* inodes[parent_inode].i_block[j] + rec + ((sizeof(struct ext2_dir_entry)+entry->name_len) + 4 - ((sizeof(struct ext2_dir_entry)+entry->name_len) % 4)) + rest);
                    struct ext2_dir_entry *possible = (struct ext2_dir_entry *)(disk + 1024* inodes[parent_inode].i_block[j] + rec+ rest);
                    printf("%s\n",possible->name );
                    if((strncmp(possible->name,victim_file_child_name,strlen(victim_file_child_name)) == 0) && (possible->name_len == strlen(victim_file_child_name)) && (possible->inode != 0)){
                        //found
                        printf("possible: [%d] rec_len: %d, name_len: %d, name: %s\n",possible->inode,possible->rec_len,possible->name_len,possible->name);
                        check_inode_bitmap(possible->inode - 1);
                        if(possible->file_type == EXT2_FT_DIR){
                            fprintf(stderr, "Cannot restore directory. \n" );
                            exit(EISDIR);
                        }
                        for (int i=0;i<12;i++){
                            check_block_bitmap(inodes[possible->inode - 1].i_block[i] - 1);
                        }
                        for (int i=0;i<12;i++){
                            restore_block_bitmap(inodes[possible->inode - 1].i_block[i] - 1);
                        }
                        restore_inode_bitmap(possible->inode - 1);
                        inodes[possible->inode - 1].i_links_count += 1;
                        inodes[possible->inode - 1].i_dtime = 0;
                        entry->rec_len = ((sizeof(struct ext2_dir_entry)+entry->name_len) + 4 - ((sizeof(struct ext2_dir_entry)+entry->name_len) % 4));
                        
                        return;
                    }
                    rest += 1;
                }
            }
            
            rec += entry->rec_len;
        }
    }
    fprintf(stderr, "%s\n","Cannot restore file" );
    exit(1);
}