#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

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