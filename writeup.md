this project bases on xv6-riscv (not xv6-k210). 

this decision is made after painful efforts to get the first point when using xv6-k210.


1. see `copy_fat_to_xv6`
we use FatFS to copy everything in sdcard to /sdcard on the default (xv6 format virtio blk) disk.

this decision is made after painful efforts to integrate the 2 drastically incompatible file system stacks. 

the downside is we cannot support linux-style `mount`

benefits are that xv6-riscv fs stack remains untouched. 


2. 
this involves 2, instead of 1, init user progs, for copy must be done after `userinit()` and before actual execution of test cases. see `user/dummy.S` and `void userinit2(void)`


3.  
we extend 
```c
struct inode* namex(char *path, int nameiparent, char *name)
```
to 
```c
struct inode* namefd(int fd, int nameiparent, char *path, char *name);
```
to support the many syscalls that require (fd, path) lookup



