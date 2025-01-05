#!/usr/bin/perl -w

# Generate usys.S, the stubs for syscalls.

print "# generated by usys.pl - do not edit\n";

print "#include \"kernel/syscall.h\"\n";

sub entry {
    my $name = shift;
    print ".global $name\n";
    print "${name}:\n";
    print " li a7, SYS_${name}\n";
    print " ecall\n";
    print " ret\n";
}
	
entry("fork");
entry("exit");
entry("wait");
entry("pipe");
entry("read");
entry("write");
entry("close");
entry("kill");
entry("exec");
entry("open");
entry("mknod");
entry("unlink");
entry("fstat");
entry("link");
entry("mkdir");
entry("chdir");
entry("dup");
entry("getpid");
entry("sbrk");
entry("sleep");
entry("uptime");
entry("shutdown");
entry("virtiodiskrw");
entry("getdents64");

# ----------------------------------------
# New system calls (based on your doc):
# ----------------------------------------
entry("getcwd");
entry("pipe2");
entry("dup3");
entry("openat");
entry("linkat");
entry("unlinkat");
entry("mkdirat");
# entry("umount2");
# entry("mount");
entry("clone");
entry("execve");
entry("wait4");
entry("getppid");
entry("brk");
# entry("munmap");
# entry("mmap");
entry("times");
entry("uname");
entry("sched_yield");
entry("gettimeofday");
entry("nanosleep");