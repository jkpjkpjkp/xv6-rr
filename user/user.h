struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int shutdown(void);
int virtiodiskrw(void*, int, int, int);
int getdents64(int, void*, int);

// ------------------------------------------------------------------
// New system calls (based on your doc):
// ------------------------------------------------------------------

// get current working directory
int getcwd(char *buf, int size);

// create pipe, with possible flags (like O_CLOEXEC, etc.)
int pipe2(int pipefd[2], int flags);

// duplicate fd to a new fd, specifying newfd
int dup3(int oldfd, int newfd, int flags);

// open a file relative to a given dirfd
int openat(int dirfd, const char *pathname, int flags, int mode);

// create a link relative to two directory fds
int linkat(int olddirfd, const char *oldpath,
           int newdirfd, const char *newpath,
           unsigned int flags);

// unlink (remove) a file relative to a directory fd
int unlinkat(int dirfd, const char *pathname, unsigned int flags);

// mkdir relative to a directory fd
int mkdirat(int dirfd, const char *pathname, int mode);

// // unmount a filesystem
// int umount2(const char *target, int flags);

// // mount a filesystem
// int mount(const char *special, const char *dir,
//           const char *fstype, unsigned long flags,
//           const void *data);

// lightweight process creation (clone)
int clone(int flags, void *stack, int *ptid, void *tls, int *ctid);

// execute a program with argv/envp
int execve(const char *pathname, char *const argv[], char *const envp[]);

// wait for a process change (more flexible than wait())
int wait4(int pid, int *status, int options);

// get parent process ID
int getppid(void);

// adjust data segment (similar to sbrk, but more “raw”)
int brk(void *addr);

// // unmap memory
// int munmap(void *start, int length);

// // map file/device into memory
// int mmap(void *start, int length, int prot, int flags, int fd, int offset);

// get process times
int times(void *tms);

// get system information
int uname(void *uts);

// yield scheduler
int sched_yield(void);

// get current time of day
int gettimeofday(void *tv, void *tz);

// high-resolution sleep
int nanosleep(const void *req, void *rem);

typedef unsigned int uint;
// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...) __attribute__ ((format (printf, 2, 3)));
void printf(const char*, ...) __attribute__ ((format (printf, 1, 2)));
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);

// umalloc.c
void* malloc(uint);
void free(void*);
