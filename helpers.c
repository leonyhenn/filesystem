#include "helpers.h"

int check_input_path(char* expected_path,char* usage){
  if(strcmp(usage,"ext2_mkdir") == 0){
    
    if ((strcmp(expected_path,"/") == 0) && (strlen(expected_path) == 1)){
    	fprintf(stderr, "Cannot recreate root directory\n");
    	exit(EEXIST);
    }
    //if path does not start with "/", exit
    if(expected_path[0] != '/'){
      fprintf(stderr, "Absolute path should start by '/'\n");
      exit(ENOENT); 
    }
  }else if((strcmp(usage,"ext2_cp") == 0)||(strcmp(usage,"ext2_ln") == 0) || (strcmp(usage,"ext2_rm") == 0)){
    //if path is root dir, exit
    if ((strcmp(expected_path,"/") == 0) && (strlen(expected_path) == 1)){
      fprintf(stderr, "root directory is not a file\n");
      exit(EEXIST);
    }
    //if path does not start with "/", exit
    if(expected_path[0] != '/'){
      fprintf(stderr, "Absolute path should start by '/'\n");
      exit(ENOENT); 
    }
    //if path does end with "/", exit
    if(expected_path[strlen(expected_path)-1] == '/'){
      fprintf(stderr, "Path should not end by '/'\n");
      exit(ENOENT); 
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

char* get_child_name(char* expected_path){
  unsigned long expected_path_size = strlen(expected_path);
  
  //copy original expected_path to a temp variable
  char *temp = (char *)calloc(1,expected_path_size * sizeof(char));  
  strncpy(temp,expected_path,expected_path_size);

  //if last char is a '/', remove it. It cannot be root cause we already did root check.
  if(expected_path[expected_path_size-1] == '/'){
    temp[expected_path_size-1] = '\0';
    expected_path_size -= 1;
  }

  //return whatever is after last slash
  temp = strrchr(temp,'/');
  if (temp[0] == '/') {
    temp++;
  }

  if(strlen(temp) >= 255){
    fprintf(stderr, "Name is too long, please use name that is shorter than 255 chars\n");
    exit(ENAMETOOLONG);
  }

  return temp;
}

int find_inode(char* path, int type){

  //if path is empty, return root node number
  if(strlen(path) == 0){
    return EXT2_ROOT_INO -1;
  }

  unsigned long path_len = strlen(path);
  char path_temp[path_len];
  strncpy(path_temp,path,path_len);

  
  //start with root inode
  int cur_inode = EXT2_ROOT_INO - 1;
  char *cur_dir;
  cur_dir = strtok(path_temp,"/");

  while (cur_dir != NULL){

    struct ext2_dir_entry *next_dir = inode_find_dir(cur_dir,cur_inode,type);

    //cannot find dir under current, while dir under current should not be none
    if(next_dir == NULL && cur_dir != NULL){
      fprintf(stderr, "File missing in path\n");
      return -1;
    }

    cur_dir = strtok(NULL,"/");
    cur_inode = (next_dir->inode) - 1;
  }
  return cur_inode;
}

struct ext2_dir_entry* inode_find_dir(char* cur_dir,int cur_inode,int type){
  for(int j = 0; j < 12; j++) {
    if((inodes[cur_inode].i_block)[j] != 0){
      int rec = 0;
      
      while(rec < EXT2_BLOCK_SIZE){
        struct ext2_dir_entry *entry = (struct ext2_dir_entry*) (disk + 1024* inodes[cur_inode].i_block[j] + rec);
        if((strncmp(entry->name,cur_dir,entry->name_len)==0) && ((entry->file_type & inode_dir_type_switch((type & 0xF000)))>0)){
          return entry;
        }
        rec += entry->rec_len;
      }
    }
  }

  return NULL;
}

int get_new_block(){
  unsigned char* block_bitmap =(unsigned char*) (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
  if(gd->bg_free_blocks_count == 0){
    fprintf(stderr, "No more free blocks\n");
    exit(ENOENT);
  }
  sb->s_free_blocks_count--; 
  gd->bg_free_blocks_count--;
  int byte;
  int bit;
  int in_use;
  int result;
  for (byte = 0; byte < 16; byte++){
        for (bit = 0; bit < 8; bit++){
          in_use = block_bitmap[byte] >> bit & 1;
          if(!in_use){
            block_bitmap[byte] |= (1 << bit);
            result = byte*8 + bit;
            memset((disk+ EXT2_BLOCK_SIZE * (result+1)), 0, EXT2_BLOCK_SIZE);
            byte=bit=17;
            break;
          }
        }
    }
  return result;
}
int get_new_inode(){
  if(gd->bg_free_inodes_count == 0){
    fprintf(stderr, "No more free inodes\n");
    exit(ENOENT);
  }
  sb->s_free_inodes_count--; 
  gd->bg_free_inodes_count--;
  int byte;
  int bit;
  int in_use;
  int result;
  for (byte = 0; byte < 4; byte++){
    for (bit = 0; bit < 8; bit++){
        in_use = inode_bitmap[byte] >> bit & 1;
        if(!in_use){
          inode_bitmap[byte] |= (1 << bit);
          result = byte*8 + bit;
          memset(inodes + result, 0, sizeof(struct ext2_inode));
          byte=bit=9;
          break;
        }
    }
  }
  return result;
}
int find_last_dir(struct ext2_dir_entry *entry, int found, int rec){
  //if dir_entry size + name_len is mult of 4
  if((sizeof(struct ext2_dir_entry)+entry->name_len) % 4 == 0){
    //then rec_len are supposed to be exact same as dir_entry size + name_len
    if(entry->rec_len > (sizeof(struct ext2_dir_entry)+entry->name_len) && (rec + entry->rec_len == EXT2_BLOCK_SIZE)){
      //if not, then this entry is the last entry
      found = 1;
    }
  //if dir_entry size + name_len is not mult of 4
  }else{
    //then rec_len are supposed to have closest 4B
    if(entry->rec_len > ((sizeof(struct ext2_dir_entry)+entry->name_len) + 4 - ((sizeof(struct ext2_dir_entry)+entry->name_len) % 4)) && (rec + entry->rec_len == EXT2_BLOCK_SIZE)){
      //if bigger than that, this is the last
      found = 1;
    }
  }
  return found;
}

int inode_dir_type_compare(int inode_file_type, int dir_file_type){
  int result = 0;
  if(inode_file_type == EXT2_S_IFLNK){
    if(dir_file_type == EXT2_FT_SYMLINK){
      result = 1;
    }
  }else if(inode_file_type == EXT2_S_IFREG){
    if(dir_file_type == EXT2_FT_REG_FILE){
      result = 1;
    }
  }else if(inode_file_type == EXT2_S_IFDIR){
    if(dir_file_type == EXT2_FT_DIR){
      result = 1;
    }
  }
  return result;
}

int inode_dir_type_switch(int inode_file_type){
  int result;
  if(inode_file_type == EXT2_S_IFLNK){
    result =   EXT2_FT_SYMLINK;
  }else if(inode_file_type == EXT2_S_IFREG){
    result =  EXT2_FT_REG_FILE;
  }else if(inode_file_type == EXT2_S_IFDIR){
    result =  EXT2_FT_DIR;
  }
  return result;
}

int block_can_fit(int rec,int last_name_len,int new_name_len){
  return (EXT2_BLOCK_SIZE - rec - ((sizeof(struct ext2_dir_entry)+last_name_len) + 4 - ((sizeof(struct ext2_dir_entry)+last_name_len) % 4)) - ((sizeof(struct ext2_dir_entry)+new_name_len) + 4 - ((sizeof(struct ext2_dir_entry)+new_name_len) % 4)));
}
void init_inode(int inode_number, int inode_type){
  inodes[inode_number].i_mode = inode_type;
}
void init_entry(struct ext2_dir_entry *entry, int inode_number, int rec_len, char* name, int file_type){
  entry->inode = inode_number + 1;
  entry->rec_len = rec_len;
  entry->name_len = strlen(name);
  strncpy(entry->name,name,strlen(name));
  entry->file_type = file_type;
  inodes[inode_number].i_links_count += 1; 
}
int add_child(int parent_inode,char *child_name,int child_type,int child_inode){
  char type;
  int allocated = 0;
  
  //if parent inode has blocks that was already used
  if(inodes[parent_inode].i_blocks > 0){
    for(int j = 0; j < (inodes[parent_inode].i_blocks / 2); j++) {
      int rec = 0;
      while(rec < EXT2_BLOCK_SIZE){
        struct ext2_dir_entry *entry = (struct ext2_dir_entry*) (disk + 1024* inodes[parent_inode].i_block[j] + rec);
        
        if((strcmp(entry->name,child_name) == 0) && inode_dir_type_compare((child_type & 0xF000),entry->file_type)){
          fprintf(stderr, "Path/File already existed\n");
          exit(EEXIST);
        }
        
        int found = 0;
        found = find_last_dir(entry, found, rec);
        
        if(found){
          //rec + dir_entry size + name_len + new dir_entry size + new name_len cannot fit into a block, jump out of the block
          if(!block_can_fit(rec,entry->name_len,strlen(child_name))){
            
            break;
          //else change dir_entry rec len and fit in new name
          }else{
            
            entry->rec_len = ((sizeof(struct ext2_dir_entry)+entry->name_len) + 4 - ((sizeof(struct ext2_dir_entry)+entry->name_len) % 4));
            rec += entry->rec_len;
            struct ext2_dir_entry *child_entry = (struct ext2_dir_entry*) (disk + 1024* inodes[parent_inode].i_block[j] + rec);

            //get a new inode
            if(child_inode < 0){
              child_inode = get_new_inode();
              init_inode(child_inode,child_type);
            }
            init_entry(child_entry, child_inode, EXT2_BLOCK_SIZE - rec, child_name, inode_dir_type_switch((child_type  & 0xF000)));
            
            allocated = 1;
            return child_inode;
          }
        }
        rec += entry->rec_len;
      }
      
    }
  }
  if(!allocated){
    int child_block = get_new_block();
    inodes[parent_inode].i_size += EXT2_BLOCK_SIZE; 
    inodes[parent_inode].i_blocks += 2;
    inodes[parent_inode].i_block[inodes[parent_inode].i_blocks/2 - 1] = child_block+1;

    struct ext2_dir_entry *child_entry = (struct ext2_dir_entry*) (disk + 1024* (child_block + 1));
    if(child_inode < 0){
      child_inode = get_new_inode();
      init_inode(child_inode,child_type);
    }
    
    init_entry(child_entry, child_inode, EXT2_BLOCK_SIZE, child_name, inode_dir_type_switch((child_type & 0xF000)));
    
    return child_inode;
  }
  fprintf(stderr, "add_child failed\n");
  exit(1);
  return -1;
}

void check_block_bitmap(int block){
  if(block_bitmap[block / 8] >> (block % 8) & 1){
    fprintf(stderr, "block %d is in use. Cannot restore file.\n",block );
    exit(EINVAL);
  }
}

void check_inode_bitmap(int inode){
  if((inode_bitmap[inode / 8] >> (inode % 8) & 1) || (inode < (EXT2_GOOD_OLD_FIRST_INO - 1) && inode > (EXT2_ROOT_INO - 1) )){
    fprintf(stderr, "inode %d is in use. Cannot restore file\n",inode );
    exit(EINVAL);
  }
}
void restore_block_bitmap(int block){
  block_bitmap[block / 8] |=  (1<<(block % 8)); 

  sb->s_free_blocks_count--; 
  gd->bg_free_blocks_count--;
}
void restore_inode_bitmap(int inode){
  inode_bitmap[inode / 8] |=  (1<<(inode % 8)); 
  sb->s_free_inodes_count--; 
  gd->bg_free_inodes_count--;
}