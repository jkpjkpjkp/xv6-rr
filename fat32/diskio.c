/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "user/user.h"

/* Definitions of physical drive number for each drive */
#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */
	

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	if (pdrv != DEV_MMC) {
		return STA_NOINIT;
	}
	return 0; // Drive is initialized and ready
}


/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(
	BYTE pdrv
)
{
	if (pdrv != DEV_MMC) {
		return STA_NOINIT;
	}
	return 0; // initialized at kernel boot
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(
	BYTE pdrv,      /* Physical drive number to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	LBA_t sector,   /* Start sector in LBA */
	UINT count      /* Number of sectors to read */
)
{
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}

	// Each virtio block is 1024 bytes (BSIZE), while FAT32 sectors are 512 bytes
	char tempbuf[1024];
	uint32 virtio_block = sector / 2;  // Convert FAT sector to virtio block number
	int offset = (sector % 2) * 512;   // Offset within the virtio block

	while (count > 0) {
		if (virtiodiskrw(tempbuf, 0, DEV_MMC, virtio_block) < 0) {
			return RES_ERROR;
		}

		// Copy the relevant 512-byte sector from the 1024-byte block
		int copy_size = 512;
		if (count == 1) {
			memcpy(buff, tempbuf + offset, copy_size);
			break;
		}

		// If we're at the first half of a block, we might need to read both halves
		if (offset == 0 && count >= 2) {
			memcpy(buff, tempbuf, 1024);
			buff += 1024;
			count -= 2;
			virtio_block++;
		} else {
			memcpy(buff, tempbuf + offset, 512);
			buff += 512;
			count--;
			if (offset == 0) {
				offset = 512;
			} else {
				offset = 0;
				virtio_block++;
			}
		}
	}

	return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0
DRESULT disk_write(
	BYTE pdrv,          /* Physical drive number to identify the drive */
	const BYTE *buff,   /* Data to be written */
	LBA_t sector,       /* Start sector in LBA */
	UINT count          /* Number of sectors to write */
)
{
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}

	char tempbuf[1024];
	uint32 virtio_block = sector / 2;
	int offset = (sector % 2) * 512;

	while (count > 0) {
		// If writing to the second half of a block or a partial block,
		// we need to read the existing block first
		if (offset == 512 || (count == 1 && offset == 0)) {
			if (virtiodiskrw(tempbuf, 0, DEV_MMC, virtio_block) < 0) {
				return RES_ERROR;
			}
		}

		if (count == 1) {
			memcpy(tempbuf + offset, buff, 512);
			if (virtiodiskrw(tempbuf, 1, DEV_MMC, virtio_block) < 0) {
				return RES_ERROR;
			}
			break;
		}

		// If we're aligned to the start of a block and have at least 2 sectors
		if (offset == 0 && count >= 2) {
			memcpy(tempbuf, buff, 1024);
			if (virtiodiskrw(tempbuf, 1, DEV_MMC, virtio_block) < 0) {
				return RES_ERROR;
			}
			buff += 1024;
			count -= 2;
			virtio_block++;
		} else {
			memcpy(tempbuf + offset, buff, 512);
			if (virtiodiskrw(tempbuf, 1, DEV_MMC, virtio_block) < 0) {
				return RES_ERROR;
			}
			buff += 512;
			count--;
			if (offset == 0) {
				offset = 512;
			} else {
				offset = 0;
				virtio_block++;
			}
		}
	}

	return RES_OK;
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl(
	BYTE pdrv,     /* Physical drive number */
	BYTE cmd,      /* Control command code */
	void *buff     /* Parameter and data buffer */
)
{
	if (pdrv != DEV_MMC) {
		return RES_PARERR;
	}

	switch (cmd) {
		case CTRL_SYNC:
			// No need for explicit sync in this implementation
			return RES_OK;

		case GET_SECTOR_SIZE:
			*(WORD*)buff = 512;  // Standard sector size for FAT32
			return RES_OK;

		case GET_BLOCK_SIZE:
			*(DWORD*)buff = 1;   // No special erase block size
			return RES_OK;

		case GET_SECTOR_COUNT:
			// You might want to adjust this based on your actual disk size
			*(DWORD*)buff = 131072; // 64MB in 512-byte sectors
			return RES_OK;

		default:
			return RES_PARERR;
	}
}

/*-----------------------------------------------------------------------*/
/* Get current time for FAT filesystem                                   */
/*-----------------------------------------------------------------------*/
DWORD get_fattime(void)
{
	// Return a fixed timestamp for simplicity
	// You could implement a real time source if needed
	return ((DWORD)(2024 - 1980) << 25) |   // Year (2024)
		   ((DWORD)3 << 21) |               // Month (March)
		   ((DWORD)1 << 16) |               // Day (1)
		   ((DWORD)12 << 11) |              // Hour (12)
		   ((DWORD)0 << 5) |                // Minute (0)
		   ((DWORD)0 >> 1);                 // Second (0)
}

