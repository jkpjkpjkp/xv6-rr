//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "virtio_disk.h"
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"
#include "stat.h"

// the address of virtio mmio register r.
#define R(r, dev) ((volatile uint32 *)(VIRTIO0 + (r) + (uint64)dev * 0x1000))

static struct disk {
  // a set (not a ring) of DMA descriptors, with which the
  // driver tells the device where to read and write individual
  // disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  struct virtq_desc *desc;

  // a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  struct virtq_avail *avail;

  // a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  struct virtq_used *used;

  // our own book-keeping.
  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used[2..NUM].

  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    struct buf *b;
    char status;
  } info[NUM];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio_blk_req ops[NUM];
  
  struct spinlock vdisk_lock;
  
} disk[2];

void
virtio_disk_init(int dev)
{
  uint32 status = 0;

  initlock(&disk[dev].vdisk_lock, "virtio_disk");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE, dev) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION, dev) != 2 ||
     *R(VIRTIO_MMIO_DEVICE_ID, dev) != 2 ||
     *R(VIRTIO_MMIO_VENDOR_ID, dev) != 0x554d4551){
    panic("could not find virtio disk");
  }
  
  // reset device
  *R(VIRTIO_MMIO_STATUS, dev) = status;

  // set ACKNOWLEDGE status bit
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS, dev) = status;

  // set DRIVER status bit
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS, dev) = status;

  // negotiate features
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES, dev);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES, dev) = features;

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS, dev) = status;

  // re-read status to ensure FEATURES_OK is set.
  status = *R(VIRTIO_MMIO_STATUS, dev);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio disk FEATURES_OK unset");

  // initialize queue 0.
  *R(VIRTIO_MMIO_QUEUE_SEL, dev) = 0;

  // ensure queue 0 is not in use.
  if(*R(VIRTIO_MMIO_QUEUE_READY, dev))
    panic("virtio disk should not be ready");

  // check maximum queue size.
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX, dev);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");

  // allocate and zero queue memory.
  disk[dev].desc = kalloc();
  disk[dev].avail = kalloc();
  disk[dev].used = kalloc();
  if(!disk[dev].desc || !disk[dev].avail || !disk[dev].used)
    panic("virtio disk kalloc");
  memset(disk[dev].desc, 0, PGSIZE);
  memset(disk[dev].avail, 0, PGSIZE);
  memset(disk[dev].used, 0, PGSIZE);

  // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM, dev) = NUM;

  // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW, dev) = (uint64)disk[dev].desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH, dev) = (uint64)disk[dev].desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW, dev) = (uint64)disk[dev].avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH, dev) = (uint64)disk[dev].avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW, dev) = (uint64)disk[dev].used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH, dev) = (uint64)disk[dev].used >> 32;

  // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY, dev) = 0x1;

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    disk[dev].free[i] = 1;

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS, dev) = status;

  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc(int dev)
{
  for(int i = 0; i < NUM; i++){
    if(disk[dev].free[i]){
      disk[dev].free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(int i, int dev)
{
  if(i >= NUM)
    panic("free_desc 1");
  if(disk[dev].free[i])
    panic("free_desc 2");
  disk[dev].desc[i].addr = 0;
  disk[dev].desc[i].len = 0;
  disk[dev].desc[i].flags = 0;
  disk[dev].desc[i].next = 0;
  disk[dev].free[i] = 1;
  wakeup(&disk[dev].free[0]);
}

// free a chain of descriptors.
static void
free_chain(int i, int dev)
{
  while(1){
    int flag = disk[dev].desc[i].flags;
    int nxt = disk[dev].desc[i].next;
    free_desc(i, dev);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int
alloc3_desc(int *idx, int dev)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc(dev);
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j], dev);
      return -1;
    }
  }
  return 0;
}

void
virtio_disk_rw(struct buf *b, int write, int dev)
{
  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&disk[dev].vdisk_lock);

  // the spec's Section 5.2 says that legacy block operations use
  // three descriptors: one for type/reserved/sector, one for the
  // data, one for a 1-byte status result.

  // allocate the three descriptors.
  int idx[3];
  while(1){
    if(alloc3_desc(idx, dev) == 0) {
      break;
    }
    sleep(&disk[dev].free[0], &disk[dev].vdisk_lock);
  }

  // format the three descriptors.
  // qemu's virtio-blk.c reads them.

  struct virtio_blk_req *buf0 = &disk[dev].ops[idx[0]];

  if(write)
    buf0->type = VIRTIO_BLK_T_OUT; // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN; // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;

  disk[dev].desc[idx[0]].addr = (uint64) buf0;
  disk[dev].desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk[dev].desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk[dev].desc[idx[0]].next = idx[1];

  disk[dev].desc[idx[1]].addr = (uint64) b->data;
  disk[dev].desc[idx[1]].len = BSIZE;
  if(write)
    disk[dev].desc[idx[1]].flags = 0; // device reads b->data
  else
    disk[dev].desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
  disk[dev].desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk[dev].desc[idx[1]].next = idx[2];

  disk[dev].info[idx[0]].status = 0xff; // device writes 0 on success
  disk[dev].desc[idx[2]].addr = (uint64) &disk[dev].info[idx[0]].status;
  disk[dev].desc[idx[2]].len = 1;
  disk[dev].desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
  disk[dev].desc[idx[2]].next = 0;

  // record struct buf for virtio_disk_intr().
  b->disk = 1;
  disk[dev].info[idx[0]].b = b;

  // tell the device the first index in our chain of descriptors.
  disk[dev].avail->ring[disk[dev].avail->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.
  disk[dev].avail->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY, dev) = 0; // value is queue number

  // Wait for virtio_disk_intr() to say request has finished.
  while(b->disk == 1) {
    sleep(b, &disk[dev].vdisk_lock);
  }

  disk[dev].info[idx[0]].b = 0;
  free_chain(idx[0], dev);

  release(&disk[dev].vdisk_lock);
}

void
virtio_disk_intr(int dev)
{
  acquire(&disk[dev].vdisk_lock);

  // the device won't raise another interrupt until we tell it
  // we've seen this interrupt, which the following line does.
  // this may race with the device writing new entries to
  // the "used" ring, in which case we may process the new
  // completion entries in this interrupt, and have nothing to do
  // in the next interrupt, which is harmless.
  *R(VIRTIO_MMIO_INTERRUPT_ACK, dev) = *R(VIRTIO_MMIO_INTERRUPT_STATUS, dev) & 0x3;

  __sync_synchronize();

  // the device increments disk[dev].used->idx when it
  // adds an entry to the used ring.

  while(disk[dev].used_idx != disk[dev].used->idx){
    __sync_synchronize();
    int id = disk[dev].used->ring[disk[dev].used_idx % NUM].id;

    if(disk[dev].info[id].status != 0)
      panic("virtio_disk_intr status");

    struct buf *b = disk[dev].info[id].b;
    b->disk = 0;   // disk is done with buf
    wakeup(b);

    disk[dev].used_idx += 1;
  }

  release(&disk[dev].vdisk_lock);
}


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
  virtio_disk_init(1);
  begin_op();
  if(create("/sdcard", T_DIR, 0, 0, 0) == 0){
    end_op();
    panic("[disk_initialize]");
  }
  end_op();
  return 0;
}

DRESULT 
disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
  if(pdrv != 1 || BSIZE != 1024)
    panic("[disk_write]");
  struct buf b;
  initsleeplock(&b.lock, "[disk_write] buf b");
  b.valid = 0;
  b.disk = 0;
  b.dev = 1;
  b.blockno = sector / 2;
  if(sector&1){
    virtio_disk_rw(&b, 0, 1);
    memmove(b.data+512, buff, 512);
    virtio_disk_rw(&b, 1, 1);
    sector ++;
    count --;
    b.blockno ++;
  }
  while(count >= 2){
    memmove(b.data, buff, BSIZE);
    virtio_disk_rw(&b, 1, 1);
    sector += 2;
    count -= 2;
    b.blockno ++;
  }
  if(count){
    virtio_disk_rw(&b, 0, 1);
    memmove(b.data, buff, 512);
    virtio_disk_rw(&b, 1, 1);
  }
  return RES_OK;
}

DRESULT 
disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
  if(pdrv != 1 || BSIZE != 1024)
    panic("[disk_read]");
  struct buf b;
  b.valid = 0;
  b.disk = 0;
  b.dev = 1;
  b.blockno = sector / 2;
  if(sector & 1){
    virtio_disk_rw(&b, 0, 1);
    memmove(buff, b.data+512, 512);
    sector ++;
    count --;
    b.blockno ++;
  }
  while(count >= 2){
    virtio_disk_rw(&b, 0, 1);
    memmove(buff, b.data+512, BSIZE);
    sector += 2;
    count -= 2;
    b.blockno ++;
  }
  if(count){
    virtio_disk_rw(&b, 0, 1);
    memmove(buff, b.data, 512);
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