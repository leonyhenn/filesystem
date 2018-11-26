#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;


int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
    printf("Block group:\n");
    printf("    block bitmap: %d\n", gd->bg_block_bitmap );
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap );
    printf("    inode table: %d\n",gd->bg_inode_table );
    printf("    free blocks: %d\n",gd->bg_free_blocks_count);
    printf("    free inodes: %d\n",gd->bg_free_inodes_count);
    printf("    used_dirs: %d\n",gd->bg_used_dirs_count);

    printf("Block bitmap: ");
    unsigned char* block_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    int byte;
    int bit;
    int in_use;
    for (byte = 0; byte < 16; byte++){
        for (bit = 0; bit < 8; bit++){
            in_use = block_bitmap[byte] >> bit & 1;
            printf("%d", in_use);
        }
        printf(" ");
    }
    printf("\n");


    printf("Inode bitmap: ");
    unsigned char* inode_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    for (byte = 0; byte < 4; byte++){
        for (bit = 0; bit < 8; bit++){
            in_use = inode_bitmap[byte] >> bit & 1;
            printf("%d", in_use);
        }
        printf(" ");
    }
    printf("\n");
    printf("\n");
    printf("Inodes:\n");
    char type;
    struct ext2_inode *inodes = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table );
    for (int i=0;i<sb->s_inodes_count;i++){
        if((inode_bitmap[i / 8] >> (i % 8) & 1) == 1){
            if(i == EXT2_ROOT_INO - 1 || i > EXT2_GOOD_OLD_FIRST_INO - 1){
                if(inodes[i].i_mode & EXT2_S_IFREG){
                    type = 'f';
                }else if (inodes[i].i_mode & EXT2_S_IFDIR){
                    type = 'd';      
                }else if(inodes[i].i_mode & EXT2_S_IFLNK){
                    type = 'l';
                }
                printf("[%d] type: %c size: %d links: %d blocks: %d\n", i + 1, type, (inodes+i)->i_size, inodes[i].i_links_count, inodes[i].i_blocks);
                printf("[%d] Blocks: ",i+1);
                for(int j = 0; j < 15; j++) {
                    if((inodes[i].i_block)[j] != 0){
                        printf(" %d", (inodes[i].i_block)[j]);
                    }
                }
                printf("\n");


            }
        }

    }
    printf("\n");
    printf("Directory Blocks:\n" ); 
    for (int i=0;i<sb->s_inodes_count;i++){
        if((inode_bitmap[i / 8] >> (i % 8) & 1) == 1){
            if(i == EXT2_ROOT_INO - 1 || i > EXT2_GOOD_OLD_FIRST_INO - 1){
                
                for(int j = 0; j < 15; j++) {
                    if((inodes[i].i_block)[j] != 0){
                        if(inodes[i].i_mode & EXT2_S_IFDIR){
                            printf("   DIR BLOCK NUM: %d (for inode %d)\n",inodes[i].i_block[j],i+1);
                            int rec = 0;
                            while(rec < EXT2_BLOCK_SIZE){
                                struct ext2_dir_entry *entry = (struct ext2_dir_entry*) (disk + 1024* inodes[i].i_block[j] + rec);
                                if(entry->file_type & EXT2_FT_REG_FILE){
                                    type = 'f';
                                }else if (entry->file_type & EXT2_FT_DIR){
                                    type = 'd';      
                                }else if(entry->file_type & EXT2_FT_DIR){
                                    type = '0';
                                }
                                printf("Inode: %d rec_len: %d name_len: %d type= %c name=%.*s\n",entry->inode,entry->rec_len,entry->name_len,type,entry->name_len, entry->name);
                                rec += entry->rec_len;
                            }
                        }
                    }
                }
            }
        }

    }
    
    return 0;
}
