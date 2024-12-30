# Initial process that execs /init.
# This code runs in user space.





.globl start
start:

# sys_initfatcopy()
        li a7, 99 # SYS_initfatcopy
        ecall

# exec(init, argv)
        la a0, init
        la a1, argv
        li a7, 221 # SYS_exec
        ecall

# for(;;) exit();
exit:
        li a7, 93 # SYS_exit
        ecall
        jal exit

# char init[] = "/init\0";
init:
  .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .quad init
  .quad 0
