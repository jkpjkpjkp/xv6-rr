#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "fat32/ff.h"

uint64
copy_file_from_fat32(char *filename)
{
  FIL fp;
  char path[MAXPATH], buf[BSIZE];
  FSIZE_t sz;
  UINT br;
  struct inode *ip;
  strncpy(path, "/sdcard/", 10);
  safestrcpy(path+8, filename, MAXPATH - 10);
  ip = create(path, T_FILE, 0, 0, 0);
  f_open(&fp, path+7, FA_READ);
  sz = f_size(&fp);
  for(int i = 0; i < sz; i += BSIZE){
    f_read (
      &fp,     /* [IN] File object */
      buf,  /* [OUT] Buffer to store read data */
      BSIZE,    /* [IN] Number of bytes to read */
      &br     /* [OUT] Number of bytes read */
    );
    writei(ip, 0, (uint64)buf, i, br);
  }
  return 0;
}

uint64
copy_all_files()
{
  FATFS fs;
  fs.pdrv = 1;
  f_mount(&fs, "1:", 1);
  DIR dp;
  FILINFO fno;
  f_opendir(&dp, "/");
  while(f_readdir(&dp, &fno)){
    if(fno.fattrib & AM_DIR)
      continue;
    copy_file_from_fat32(fno.fname);
  }
  return 0;
}

char *tests[] = {
  "brk",
  "chdir",
  "clone",
  "close",
  "dup2",
  "dup",
  "execve",
  "exit",
  "fork",
  "fstat",
  "getcwd",
  "getdents",
  "getpid",
  "getppid",
  "gettimeofday",
  "mkdir_",
  "mmap",
  "mount",
  "munmap",
  "openat",
  "open",
  "pipe",
  "read",
  "times",
  "umount",
  "uname",
  "unlink",
  "wait",
  "waitpid",
  "write",
  "yield",
  0
};

int
main(int argc, char *argv[])
{
  int pid, wpid;
  char path[64];
  char *args[2];  // Array of pointers for exec arguments

  for(char **test = tests; *test; test++) {
    printf("Testing %s:\n", *test);
    
    // Use safer string copy
    memmove(path, "/sdcard/\0", 9);
    memmove(path + 8, *test, strlen(*test) + 1);

    pid = fork();
    if(pid < 0) {
      printf("runall: fork failed\n");
      exit(1);
    }
    if(pid == 0) {
      args[0] = path;    // First argument is program name
      args[1] = 0;       // Null terminate the array
      exec(path, args);  // Pass array of pointers
      printf("runall: exec %s failed\n", path);
      exit(1);
    }

    // Parent waits
    while((wpid = wait(0)) >= 0 && wpid != pid) {
      // Wait for the specific child
    }
  }
  shutdown();
}