#include "helpers.h"

unsigned char *disk;


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

    //check path
    check_input_path(expected_path,"ext2_mkdir");

    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);

    //get parent dir
    get_parent_directory(expected_path);
    printf("%s\n",parent_dir );

    

}