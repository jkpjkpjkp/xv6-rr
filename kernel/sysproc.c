#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

void
sys_shutdown(void)
{
  // Use SBI SRST (System Reset) extension to shutdown
  // SBI_SYSTEM_RESET = 0x53525354 (ASCII "SRST")
  // Type = 0 for shutdown
  // Reason = 0 for normal shutdown
  asm volatile("li a7, 0x53525354"); // SBI SRST extension
  asm volatile("li a0, 0"); // Type = shutdown
  asm volatile("li a1, 0"); // Reason = normal
  asm volatile("ecall");
  panic("shutdown reached end");
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

struct timespec {
	long tv_sec;        /* 秒 */
	long   tv_nsec;       /* 纳秒, 范围在0~999999999 */
};

uint64
sys_nanosleep(void)
{
  struct timespec req;
  uint64 req_addr;
  uint ticks0;
  long total_ticks;

  // Get pointer to timespec struct from user
  argaddr(0, &req_addr);
  if(copyin(myproc()->pagetable, (char *)&req, req_addr, sizeof(req)) < 0)
    return -1;

  // Convert seconds and nanoseconds to ticks
  // 1 tick = 100ms = 100,000,000ns = 0.1s
  total_ticks = (req.tv_sec * 10) + (req.tv_nsec / 100000000);
  if(total_ticks < 0)
    total_ticks = 0;

  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < total_ticks){
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

struct tms              
{                     
	long tms_utime;  
	long tms_stime;  
	long tms_cutime; 
	long tms_cstime; 
};

uint64
sys_times(void)
{
  uint64 addr;
  struct tms buf;
  uint xticks;

  argaddr(0, &addr);

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  buf.tms_utime = xticks;  // User CPU time
  buf.tms_stime = xticks;  // System CPU time
  buf.tms_cutime = 0;      // User CPU time of terminated child processes
  buf.tms_cstime = 0;      // System CPU time of terminated child processes

  if(copyout(myproc()->pagetable, addr, (char *)&buf, sizeof(buf)) < 0)
    return -1;

  return ticks; // Return total system uptime in clock ticks
}

struct timeval
{
    uint64 sec;  // 自 Unix 纪元起的秒数
    uint64 usec; // 微秒数
};

uint64
sys_gettimeofday(void)
{
  uint64 addr;
  struct timeval tv;
  uint xticks;

  argaddr(0, &addr);

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  // Convert ticks to seconds and microseconds
  tv.sec = xticks / 100; // 100 ticks per second
  tv.usec = (xticks % 100) * 10000; // Convert remainder to microseconds

  if(copyout(myproc()->pagetable, addr, (char *)&tv, sizeof(tv)) < 0)
    return -1;

  return 0;
}

uint64
sys_uname(void)
{
  return -1;  // Dummy implementation
}

uint64
sys_sched_yield(void)
{
  yield();
  return 0;
}

uint64
sys_wait4(void)
{
  return -1;  // Dummy implementation
}
