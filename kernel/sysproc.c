#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

void
sys_shutdown(void)
{ // TODO not right. fine sbi shutdown. 
  // Shutdown the system by writing to the TEST register
  // This is specific to QEMU's RISC-V virt machine
  *(uint32*)0x100000 = 0x5555; 
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_getppid(void)
{
  return myproc()->parent->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr, brk;

  argaddr(0, &brk);
  addr = myproc()->sz;
  if(growproc((int)(brk-addr)) < 0)
    return -1;
  return addr;
}

uint64
sys_brk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_sched_yield(void)
{
  yield();
  return 0;
}

uint64
sys_nanosleep(void)
{
  return -1;
}

uint64
sys_wait4(void)
{
  return -1;  // Dummy implementation
}

uint64
sys_execve(void)
{
  return -1;  // Dummy implementation
}

uint64
sys_times(void)
{
  return -1;  // Dummy implementation
}

uint64
sys_gettimeofday(void)
{
  return -1;  // Dummy implementation
}

uint64
sys_uname(void)
{
  return -1;  // Dummy implementation
}