#include "helpers.h"

int check_input_path(char* expected_path,char* usage){
  if (strcmp(expected_path,"/")){
  	fprintf(stderr, "Usage: ext2_mkdir <image file name> <path>\n");
  	return -EEXIST;
  	exit(1);

  }

  return 1;
}

