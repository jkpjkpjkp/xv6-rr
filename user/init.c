#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/param.h"
#include "fat32/ff.h"

uint64
copy_file_from_fat32(char *filename)
{
  FIL fp;
  char path[MAXPATH], buf[BSIZE];
  FSIZE_t sz;
  UINT br;
  int fd;

  // Create path for the file
  memmove(path, "/sdcard/", 8);
  memmove(path+8, filename, strlen(filename)+1);
  
  // Open destination file
  fd = open(path, O_CREATE | O_WRONLY);
  if(fd < 0) {
    printf("failed to create %s\n", path);
    return -1;
  }

  // Open source file from FAT32
  if(f_open(&fp, path+7, FA_READ) != FR_OK) {
    close(fd);
    return -1;
  }

  // Get file size
  sz = f_size(&fp);

  // Copy data
  for(int i = 0; i < sz; i += BSIZE) {
    if(f_read(&fp, buf, BSIZE, &br) != FR_OK) {
      close(fd);
      f_close(&fp);
      return -1;
    }
    if(write(fd, buf, br) != br) {
      close(fd);
      f_close(&fp);
      return -1;
    }
  }

  // Clean up
  close(fd);
  f_close(&fp);
  return 0;
}

uint64
copy_all_files()
{
  FATFS fs;
  DIR dp;
  FILINFO fno;
  mkdir("/sdcard");
  fs.pdrv = 1;
  f_mount(&fs, "1:", 1);
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

  write(1, "main() called\n", 14);
  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr
  write(1, "Direct write test\n", 17);  // Try direct write to fd 1 (stdout)
  printf("[user/init.c:main] starting\n");
  write(2, "stderr test\n", 12);        // Try writing to stderr
  
  // Force flush of stdout
  fprintf(1, "Forced stdout: init starting\n");
  
  while(1)
    ;
  return 0;
  copy_all_files();
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