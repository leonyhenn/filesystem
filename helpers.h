#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "errno.h"
extern unsigned char *disk; 
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *gd;
extern unsigned char* block_bitmap;
extern unsigned char* inode_bitmap;
extern struct ext2_inode *inodes;
int check_input_path(char* expected_path,char* usage);
char* get_parent_directory(char* expected_path);
char* get_child_name(char* expected_path);
int find_inode(char* path,int type);
int find_last_dir(struct ext2_dir_entry *entry, int found, int rec);
struct ext2_dir_entry* inode_find_dir(char* cur_dir,int cur_inode,int type);
int add_child(int parent_inode,char *child_name,int child_type,int child_inode);
int get_new_block();
int get_new_inode();
int block_can_fit(int rec,int last_name_len,int new_name_len);
int inode_dir_type_compare(int inode_file_type, int dir_file_type);
void init_inode(int inode_number, int inode_type);
void init_entry(struct ext2_dir_entry *entry, int inode_number, int rec_len, char* name, int file_type);
int inode_dir_type_switch(int inode_file_type);
void check_block_bitmap(int block);
void check_inode_bitmap(int inode);
void restore_block_bitmap(int block);
void restore_inode_bitmap(int inode);