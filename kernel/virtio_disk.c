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
virtio_disk_init(int dev) // 0 for original disk, 1 for fatfs
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

  // printf("virtio disk %d init done\n", dev);
  // printf("virtio disk %d desc: %p\n", dev, disk[dev].desc);
  // printf("virtio disk %d avail: %p\n", dev, disk[dev].avail);
  // printf("virtio disk %d used: %p\n", dev, disk[dev].used);
  // printf("virtio disk %d status: 0x%x\n", dev, status);
  // printf("virtio disk %d queue num: %d\n", dev, NUM);
  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc(int dev)
{
  // if(dev == 1) {
  //   for(int i = 0; i < NUM; i++) {
  //     printf("desc %d: free=%d addr=%lu len=%d flags=%d next=%d\n",
  //            i, disk[dev].free[i], disk[dev].desc[i].addr,
  //            disk[dev].desc[i].len, disk[dev].desc[i].flags,
  //            disk[dev].desc[i].next);
  //   }
  //   while(1)
  //     ;
  // }
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
  // printf("[virtio_disk_rw] starting dev=%d blockno=%d write=%d\n",
        //  dev, b->blockno, write);

  // Check buffer pointer range or alignment if you suspect an invalid pointer.
  // Replace KERNBASE/PHYSTOP with whatever region is valid for your system.
  // #ifdef DEBUG
  //   if ((uint64)b->data < KERNBASE || (uint64)b->data > PHYSTOP) {
  //     printf("[virtio_disk_rw] WARNING: b->data out of valid range: 0x%lx\n",
  //            (uint64)b->data);
  //   }
  // #endif

  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&disk[dev].vdisk_lock);

  int idx[3];

  while (1) {
    if (alloc3_desc(idx, dev) == 0) {
      break;
    }
    sleep(&disk[dev].free[0], &disk[dev].vdisk_lock);
  }

  // printf("[virtio_disk_rw] sector=%lu, write=%d\n", sector, write);
  // printf("[virtio_disk_rw] Descriptor indices allocated: %d, %d, %d\n",
        //  idx[0], idx[1], idx[2]);

  struct virtio_blk_req *buf0 = &disk[dev].ops[idx[0]];

  if (write)
    buf0->type = VIRTIO_BLK_T_OUT; // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN;  // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;

  disk[dev].desc[idx[0]].addr = (uint64) buf0;
  disk[dev].desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk[dev].desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk[dev].desc[idx[0]].next = idx[1];

  disk[dev].desc[idx[1]].addr = (uint64) b->data;
  disk[dev].desc[idx[1]].len = BSIZE;
  if (write)
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

  // Additional debug: print contents of the "avail" ring after updating.
  // printf("[virtio_disk_rw] avail->idx=%d\n", disk[dev].avail->idx);

  *R(VIRTIO_MMIO_QUEUE_NOTIFY, dev) = 0; // queue number

  // Wait for virtio_disk_intr() to finish
  while (b->disk == 1) {
    // If you really suspect concurrency/timing, you can print here.
    // printf("[virtio_disk_rw] waiting for completion dev=%d blockno=%d\n", dev, b->blockno);
    sleep(b, &disk[dev].vdisk_lock);
  }

  // Thorough debug info after the request completes
  // printf("[virtio_disk_rw] Request completed, printing debug info:\n");
  // printf("  Device: %d\n", dev);
  // printf("  Block number: %d\n", b->blockno);
  // printf("  Write operation: %d\n", write);
  // printf("  Descriptor chain indices: %d, %d, %d\n", idx[0], idx[1], idx[2]);

  // printf("  Descriptor 0: addr=0x%lx len=%d flags=0x%x next=%d\n", 
  //        disk[dev].desc[idx[0]].addr,
  //        disk[dev].desc[idx[0]].len,
  //        disk[dev].desc[idx[0]].flags,
  //        disk[dev].desc[idx[0]].next);

  // printf("  Descriptor 1: addr=0x%lx len=%d flags=0x%x next=%d\n",
  //        disk[dev].desc[idx[1]].addr, 
  //        disk[dev].desc[idx[1]].len,
  //        disk[dev].desc[idx[1]].flags,
  //        disk[dev].desc[idx[1]].next);

  // printf("  Descriptor 2: addr=0x%lx len=%d flags=0x%x next=%d\n",
  //        disk[dev].desc[idx[2]].addr,
  //        disk[dev].desc[idx[2]].len, 
  //        disk[dev].desc[idx[2]].flags,
  //        disk[dev].desc[idx[2]].next);

  // printf("  Status byte: 0x%x\n", disk[dev].info[idx[0]].status);
  // printf("  Avail->idx: %d\n", disk[dev].avail->idx);
  // printf("  Used->idx: %d\n", disk[dev].used_idx);
  // printf("  b->disk flag (should be 0 now): %d\n", b->disk);

  // Print first 16 bytes of data buffer
  // printf("  Data buffer (first 16 bytes): ");
  // for (int i = 0; i < 16; i++) {
  //   printf("%02x ", ((unsigned char*)b->data)[i]);
  // }
  // printf("\n");

  disk[dev].info[idx[0]].b = 0;
  free_chain(idx[0], dev);

  release(&disk[dev].vdisk_lock);

  // Post-check for read requests: see if it's suspiciously empty
  if (!write) {
    int all_zeros = 1;
    unsigned char *data = (unsigned char*)b->data;
    for (int i = 0; i < BSIZE; i++) {
      if (data[i] != 0) {
        all_zeros = 0;
        break;
      }
    }
    if (all_zeros) {
      printf("[virtio_disk_rw] WARNING: read buffer is all zeros\n");
    }
  }
}

void
virtio_disk_intr(int dev)
{
  acquire(&disk[dev].vdisk_lock);

  // Acknowledge interrupt
  uint32 status_reg = *R(VIRTIO_MMIO_INTERRUPT_STATUS, dev);
  *R(VIRTIO_MMIO_INTERRUPT_ACK, dev) = status_reg & 0x3;

  __sync_synchronize();

  // Debug: Show current used->idx vs device used->idx
  // The device increments used->idx in the "used" ring once it is done.
  // printf("[virtio_disk_intr] dev=%d status_reg=0x%x used_idx(local)=%d used->idx(device)=%d\n",
        //  dev, status_reg, disk[dev].used_idx, disk[dev].used->idx);

  while (disk[dev].used_idx != disk[dev].used->idx) {
    __sync_synchronize();
    int id = disk[dev].used->ring[disk[dev].used_idx % NUM].id;

    // If device wrote non-zero status, log it
    if (disk[dev].info[id].status != 0) {
      // printf("[virtio_disk_intr] ERROR: status for id %d = 0x%x\n",
            //  id, disk[dev].info[id].status);
      panic("virtio_disk_intr status");
    }

    struct buf *b = disk[dev].info[id].b;
    b->disk = 0; // disk is done with buf
    wakeup(b);

    disk[dev].used_idx += 1;

    // Additional debug
    // printf("[virtio_disk_intr] Completed descriptor id=%d, next used_idx=%d\n",
    //        id, disk[dev].used_idx);
  }

  release(&disk[dev].vdisk_lock);
}
