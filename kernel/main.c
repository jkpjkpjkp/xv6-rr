#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "fat32/ff.h"
#include "stat.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    printf("DEBUG: Starting console init\n");
    consoleinit();
    printf("DEBUG: Console initialized\n");
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    printf("DEBUG: Starting kinit\n");
    kinit();         // physical page allocator
    printf("DEBUG: Starting kvminit\n");
    kvminit();       // create kernel page table
    printf("DEBUG: Starting kvminithart\n");
    kvminithart();   // turn on paging
    printf("DEBUG: Starting procinit\n");
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    printf("DEBUG: Starting plicinit\n");
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    printf("DEBUG: Starting virtio_disk_init\n");
    virtio_disk_init(0); // emulated hard disk
    userinit(0);      // first user process
    __sync_synchronize();
    started = 1;
    copy_all_files();
    userinit(1);
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}