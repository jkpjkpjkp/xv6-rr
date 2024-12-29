// Sleeping locks

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
  printf("[acquiresleep] starting\n");
  printf("[acquiresleep] %lld\n", (long long int)&lk->lk);
  acquire(&lk->lk);
  printf("[acquiresleep] acquired %u\n", lk->locked);
  while (lk->locked) {
    printf("[acquiresleep] sleeping\n");
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;

  printf("[acquiresleep] myproc\n");
  lk->pid = myproc()->pid;

  printf("[acquiresleep] release\n");
  release(&lk->lk);
  printf("[acquiresleep] done\n");
}

void
releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  release(&lk->lk);
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}



