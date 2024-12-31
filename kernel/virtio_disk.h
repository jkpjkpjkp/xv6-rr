#include "types.h"
#include "fat32/ff.h"
#include "fat32/diskio.h"
typedef unsigned char BYTE;
typedef unsigned int UINT;
#ifndef DWORD
typedef uint32 DWORD;
#endif
typedef unsigned short WORD;
typedef BYTE DSTATUS;
#define STA_NOINIT 0x01  // Drive not initialized
#define RES_OK 0         // Successful
#define RES_PARERR 1     // Parameter error
#ifndef LBA_t
typedef DWORD LBA_t;
#endif
#define CTRL_SYNC 0
#define GET_SECTOR_COUNT 1
#define GET_SECTOR_SIZE 2
#define GET_BLOCK_SIZE 3
#define TOTAL_SECTORS 2048 * 8 // 8MB

DSTATUS disk_status(BYTE pdrv);
DSTATUS disk_initialize(BYTE pdrv);
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count);
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count);
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);
DWORD get_fattime(void);