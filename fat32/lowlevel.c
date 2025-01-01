#include "ff.h"
#include "ffconf.h"
#include "diskio.h"
#include "ffcompat.h"
#include "user/user.h"
#include "kernel/fs.h"

DSTATUS 
disk_status(BYTE pdrv) {
  if(pdrv != 1)
    panic("[disk_status]");
  return 0;
}

DSTATUS 
disk_initialize(BYTE pdrv) {
  if(pdrv != 1)
    panic("[disk_initialize]");
  // disk initialized at kernel boot in kernel/main.c.  
  return 0;
}

DRESULT 
disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
  if(pdrv != 1 || BSIZE != 1024)
    panic("[disk_write]");

  struct {
    void *buf;
    int write; 
    int dev;
    int blockno;
  } args;

  args.buf = (void*)buff;
  args.write = 1;  // write
  args.dev = 1;    // second disk
  args.blockno = sector / 2;

  if(sector & 1) {
    syscall(SYS_virtio_disk_rw, (uint64)&args);
    memmove(args.buf+512, buff, 512);
    syscall(SYS_virtio_disk_rw, (uint64)&args);
    sector++;
    count--;
    args.blockno++;
  }

  while(count >= 2) {
    memmove(args.buf, buff, BSIZE);
    syscall(SYS_virtio_disk_rw, (uint64)&args);
    sector += 2;
    count -= 2;
    args.blockno++;
  }

  if(count) {
    syscall(SYS_virtio_disk_rw, (uint64)&args);
    memmove(args.buf, buff, 512);
    syscall(SYS_virtio_disk_rw, (uint64)&args);
  }

  return RES_OK;
}

DRESULT 
disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
  if(pdrv != 1 || BSIZE != 1024)
    panic("[disk_read]");
  
  // Use syscall instead of direct virtio access
  struct {
    void *buf;
    int write;
    int dev;
    int blockno;
  } args;

  args.buf = buff;
  args.write = 0;  // read
  args.dev = 1;    // second disk
  args.blockno = sector / 2;

  if(sector & 1) {
    syscall(SYS_virtio_disk_rw, (uint64)&args);
    memmove(buff, args.buf+512, 512);
    sector++;
    count--;
    args.blockno++;
  }

  while(count >= 2) {
    syscall(SYS_virtio_disk_rw, (uint64)&args);
    memmove(buff, args.buf, BSIZE);
    sector += 2;
    count -= 2;
    args.blockno++;
  }

  if(count) {
    syscall(SYS_virtio_disk_rw, (uint64)&args);
    memmove(buff, args.buf, 512);
  }

  return RES_OK;
}

DRESULT 
disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
  switch (cmd) {
    case CTRL_SYNC:
      // Ensure all write operations are completed
      return RES_OK;
    case GET_SECTOR_COUNT:
      // Return the total number of sectors
      *(DWORD *)buff = TOTAL_SECTORS;
      return RES_OK;
    case GET_SECTOR_SIZE:
      // Return the sector size
      *(WORD *)buff = 512;
      return RES_OK;
    case GET_BLOCK_SIZE:
      // Return the block size
      *(DWORD *)buff = 1; // Assuming no erase block
      return RES_OK;
    default:
        return RES_PARERR;
  }
}

DWORD get_fattime(void) {
    // Return a fixed time for simplicity
    return ((DWORD)(2024 - 1980) << 25) | ((DWORD)12 << 21) | ((DWORD)31 << 16);
}