#ifndef __FLASH_H
#define __FLASH_H

DSTATUS disk_status_flash(void);
DRESULT disk_read_flash ( BYTE* buff, DWORD sector, UINT count);
DRESULT disk_write_flash (const BYTE* buff, DWORD sector, UINT count);
DSTATUS disk_initialize_flash (void);
DRESULT disk_ioctl_flash (uint8_t ctrl,	void *buff);


#endif
