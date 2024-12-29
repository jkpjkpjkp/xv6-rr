# Dummy process that just exits.
# This code runs in user space.


# for(;;) exit();
exit:
        li a7, 93 // SYS_exit. r.f. kernel/syscall.h
        ecall
        jal exit