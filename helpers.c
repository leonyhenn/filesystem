#include "helpers.h"

int check_input_path(char* expected_path,char* usage){
  if(strcmp(usage,"ext2_mkdir") == 0){
    //if path is root dir, exit
    if (strcmp(expected_path,"/") == 0){
    	fprintf(stderr, "Cannot recreate root directory\n");
    	exit(1);
    }
    
    //if path does not start with "/", exit
    if(expected_path[0] != '/'){
      fprintf(stderr, "Absolute path should start by '/'\n");
      exit(1); 
    }
  }

  return 1;
}

char* get_parent_directory(char* expected_path){
  unsigned long expected_path_size = strlen(expected_path);
  
  //copy original expected_path to a temp variable
  char *temp = (char *)calloc(1,expected_path_size * sizeof(char));  
  strncpy(temp,expected_path,expected_path_size);

  //if last char is a '/', remove it. It cannot be a root cause we already did root check.
  if(expected_path[expected_path_size-1] == '/'){
    temp[expected_path_size-1] = '\0';
    expected_path_size -= 1;
  }

  //cut whatever is after last slash
  char* after_last_slash = strrchr(temp,'/');
  temp[expected_path_size-strlen(after_last_slash)] = '\0';
  
  return temp;
}
