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
  printf("[copy_file_from_fat32] %s\n", filename);
  FIL fp;
  char path[MAXPATH], buf[BSIZE];
  FSIZE_t sz;
  UINT br;
  int fd;

  // Create path for the file
  memmove(path, "/sdcard/", 8);
  memmove(path+8, filename, strlen(filename)+1);
  printf("[user/init.c:copy_file_from_fat32] %s\n", path);
  
  // Open destination file
  fd = open(path, O_CREATE | O_WRONLY);
  if(fd < 0) {
    printf("failed to create %s\n", path);
    return -1;
  }
  printf("[user/init.c:copy_file_from_fat32] before f_open\n");
  // Open source file from FAT32
  if(f_open(&fp, path+7, FA_READ) != FR_OK) {
    close(fd);
    return -1;
  }
  while(1)
    ;
  printf("[user/init.c:copy_file_from_fat32] after f_open\n");

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
  printf("[user/init.c:copy_all_files] starting\n");
  FATFS fs;
  DIR dp;
  FILINFO fno;
  mkdir("/sdcard");
  printf("[user/init.c:copy_all_files] made dir\n");
  fs.pdrv = 1;
  f_mount(&fs, "1:", 1);
  printf("[user/init.c:copy_all_files] mounted\n");
  f_opendir(&dp, "1:/");
  printf("[user/init.c:copy_all_files] f_opendir\n");
  printf("[user/init.c:copy_all_files] dp.obj.fs=0x%p\n", dp.obj.fs);
  printf("[user/init.c:copy_all_files] dp.obj.id=%d\n", dp.obj.id);
  printf("[user/init.c:copy_all_files] dp.obj.attr=%d\n", dp.obj.attr);
  printf("[user/init.c:copy_all_files] dp.obj.sclust=%u\n", dp.obj.sclust);
  printf("[user/init.c:copy_all_files] dp.obj.objsize=%u\n", dp.obj.objsize);
  printf("[user/init.c:copy_all_files] dp.obj.stat=%d\n", dp.obj.stat);
  printf("[user/init.c:copy_all_files] dp.dptr=%u\n", dp.dptr);
  printf("[user/init.c:copy_all_files] dp.clust=%u\n", dp.clust);
  printf("[user/init.c:copy_all_files] dp.sect=%u\n", dp.sect);
  printf("[user/init.c:copy_all_files] dp.dir=%p\n", dp.dir);
  printf("[user/init.c:copy_all_files] dp.fn=%p\n", dp.fn);
  while(f_readdir(&dp, &fno) == FR_OK){
    if(fno.fname[0] == '\0')
      break;

    printf("[user/init.c:copy_all_files] 1\n");
    printf("[user/init.c:copy_all_files] File found: %s\n", fno.fname);
    printf("[user/init.c:copy_all_files] Attributes: 0x%02x (%s%s%s%s%s)\n", 
           fno.fattrib,
           (fno.fattrib & AM_DIR) ? "Directory " : "",
           (fno.fattrib & AM_RDO) ? "Read-only " : "",
           (fno.fattrib & AM_HID) ? "Hidden " : "",
           (fno.fattrib & AM_SYS) ? "System " : "",
           (fno.fattrib & AM_ARC) ? "Archive " : "");
    printf("[user/init.c:copy_all_files] Size: %u bytes\n", fno.fsize);
    printf("[user/init.c:copy_all_files] Modified: %u/%02u/%02u %02u:%02u:%02u\n",
           (fno.fdate >> 9) + 1980, (fno.fdate >> 5) & 15, fno.fdate & 31,
           fno.ftime >> 11, (fno.ftime >> 5) & 63, (fno.ftime & 31) * 2);
    if(fno.fattrib & AM_DIR){
      continue;

    }

     char *p;
     for(p = fno.fname; *p; p++){
       if('A' <= *p && *p <= 'Z')
         *p = *p - 'A' + 'a';
     }
    copy_file_from_fat32(fno.fname);
    printf("[user/init.c:copy_all_files] Copied file: %s\n", fno.fname);
    // while(1)
    //   ;
    // if ('a' <= *fno.fname && *fno.fname <= 'z') {
    //   copy_file_from_fat32(fno.fname);
    //   printf("[user/init.c:copy_all_files] Copied file: %s\n", fno.fname);
    //   while(1)
    //     ;
    // }
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
  
  // while(1)
  //   ;
  // return 0;
  printf("[user/init.c:main] copy_all_files\n");
  copy_all_files();
  printf("[user/init.c:main] copied_all_files\n");

  int pid, wpid;
  char path[64];
  char *args[2];  // Array of pointers for exec arguments

  for(char **test = tests; *test; test++) {
    printf("[user/init.c:main] Testing %s:\n", *test);
    
    // Use safer string copy
    memmove(path, "/sdcard/\0", 9);
    memmove(path + 8, *test, strlen(*test) + 1);

    pid = fork();
    if(pid < 0) {
      printf("[user/init.c:main] runall: fork failed\n");
      exit(1);
    }
    if(pid == 0) {
      args[0] = path;    // First argument is program name
      args[1] = 0;       // Null terminate the array
      exec(path, args);  // Pass array of pointers
      printf("[user/init.c:main] runall: exec %s failed\n", path);
      exit(1);
    }

    printf("[user/init.c:main] waiting\n");
    // Parent waits
    while((wpid = wait(0)) >= 0 && wpid != pid) {
      // Wait for the specific child
    }
    printf("[user/init.c:main] done waiting\n");
  }
  shutdown();
}
