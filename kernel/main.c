#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "fat32/ff.h"
#include "stat.h"

volatile static int started = 0;

void init_fat_copy(void);

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    init_fat_copy();
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
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

// Helper function to copy FAT files to xv6 fs
static void
copy_fat_to_xv6(const char *fat_path, const char *xv6_path) {
  FATFS fs;
  DIR dir;
  FILINFO fno;
  FIL fsrc;
  struct inode *dst_ip;
  struct file *f;
  char buf[512];
  UINT br;
  char src_path[MAXPATH];
  char dst_path[MAXPATH];

  // Mount FAT filesystem
  if (f_mount(&fs, "", 1) != FR_OK) {
    panic("fatfs: mount failed");
  }

  // Open source directory
  if (f_opendir(&dir, fat_path) != FR_OK) {
    panic("fatfs: cannot open source dir");
  }

  // Read directory entries
  while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
    // Skip . and ..
    if (fno.fname[0] == '.') continue;

    // Only handle files (not directories)
    if (!(fno.fattrib & AM_DIR)) {
      // Build paths
      safestrcpy(src_path, fat_path, sizeof(src_path));
      safestrcpy(src_path + strlen(src_path), "/", 2);
      safestrcpy(src_path + strlen(src_path), fno.fname, sizeof(src_path) - strlen(src_path));
      
      safestrcpy(dst_path, xv6_path, sizeof(dst_path));
      safestrcpy(dst_path + strlen(dst_path), "/", 2);
      safestrcpy(dst_path + strlen(dst_path), fno.fname, sizeof(dst_path) - strlen(dst_path));

      // Open source file
      if (f_open(&fsrc, src_path, FA_READ) != FR_OK) {
        printf("fatfs: cannot open source file %s\n", src_path);
        continue;
      }

      // Create destination file
      begin_op();
      dst_ip = create(dst_path, T_FILE, 0, 0);
      if (dst_ip == 0) {
        printf("xv6fs: cannot create %s\n", dst_path);
        f_close(&fsrc);
        continue;
      }
      iunlockput(dst_ip);
      end_op();

      // Copy data
      while (f_read(&fsrc, buf, sizeof(buf), &br) == FR_OK && br > 0) {
        begin_op();
        if ((f = filealloc()) == 0) {
          break;
        }
        if (filewrite(f, (uint64)buf, br) != br) {
          printf("xv6fs: write error\n");
          break;
        }
        fileclose(f);
        end_op();
      }

      f_close(&fsrc);
      printf("Copied: %s -> %s\n", src_path, dst_path);
    }
  }

  f_closedir(&dir);
  f_unmount("");
}

// Add this to kinit() or main()
void
init_fat_copy(void) {
  // Create destination directory if it doesn't exist
  begin_op();
  struct inode *ip = create("/sdcard", T_DIR, 0, 0);
  if(ip != 0) {
    iunlockput(ip);
  }
  end_op();

  // Copy files from FAT32 to xv6 fs
  copy_fat_to_xv6("/", "/sdcard");
}