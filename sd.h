#ifndef __SD_H
#define __SD_H

DSTATUS disk_initialize_sd (void);
DSTATUS disk_status_sd (void);
DRESULT disk_read_sd ( BYTE* buff, DWORD sector, UINT count);
DRESULT disk_write_sd (const uint8_t *buff,uint32_t sector, unsigned int count);
DRESULT disk_ioctl_sd (	uint8_t ctrl,void *buff );

#endif
