# filesystem

## School Assignment

Implemented multiple functions on a ext2 file system in C language, includes: 
  - [make directory](https://github.com/leonyhenn/filesystem/blob/master/ext2_mkdir.c), equivalent to mkdir in Linux
  - [copy](https://github.com/leonyhenn/filesystem/blob/master/ext2_cp.c), equivalent to cp in Linux
  - [create hard link](https://github.com/leonyhenn/filesystem/blob/master/ext2_ln.c), equivalent to ln in Linux
  - [remove](https://github.com/leonyhenn/filesystem/blob/master/ext2_rm.c), equivalent to rm in Linux
  - [restore](https://github.com/leonyhenn/filesystem/blob/master/ext2_restore.c), to restore a removed file
  - [checker](https://github.com/leonyhenn/filesystem/blob/master/ext2_checker.c), to check if the disk is in right format

I tried to make the code as highly modulized and low cohesive as possible, so check out my utility functions:
  - [helpers](https://github.com/leonyhenn/filesystem/blob/master/helpers.c)
  
There are only a little comments in those code, it is not that I do not want to make them unmaintainable, but the clock is ticking. This _IS_ the generally acknowledged
 most time consuming project among all CS courses in UofT.
 
Also, please don't mind the commit comment being so meaningless, this is due to the editor in my school's computer quits all the time, and I have to use Github to transfer the code back and forth.
  
### Mark : 81.8%
