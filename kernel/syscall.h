// System call numbers
#define SYS_fork    1
#define SYS_clone   220
#define SYS_exit    93
#define SYS_wait    3
#define SYS_pipe2   59
#define SYS_pipe    SYS_pipe2
#define SYS_read    63
#define SYS_kill    6
#define SYS_exec    221
#define SYS_execve  SYS_exec
#define SYS_fstat   80
#define SYS_chdir   49
#define SYS_getdir  17
#define SYS_getcwd  SYS_getdir
#define SYS_getdents64  61
#define SYS_dup     23
#define SYS_dup3    24
#define SYS_getpid  172
#define SYS_getppid 173
#define SYS_sbrk    12
#define SYS_brk     214
#define SYS_sleep   13
#define SYS_uptime  14
#define SYS_open    15
#define SYS_write   64
#define SYS_mknod   217
#define SYS_unlink  18
#define SYS_unlinkat 35
#define SYS_link    19
#define SYS_mkdir   20
#define SYS_mkdirat  34
#define SYS_close   57

// File operations
#define SYS_openat   56
#define SYS_linkat   37
// #define SYS_mount    40
// #define SYS_umount2  39
// mount not supported

// Process management
#define SYS_wait4    260

// Memory management
#define SYS_mmap     222
#define SYS_munmap   215

// Other system calls
#define SYS_times    153
#define SYS_uname    160
#define SYS_sched_yield 124
#define SYS_gettimeofday 169
#define SYS_nanosleep 101


#define SYS_shutdown 77
