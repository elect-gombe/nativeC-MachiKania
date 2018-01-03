/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>



#include "ff.h"
#include "diskio.h"
#include <string.h>
#define _BUFFER_SIZE_KiB 80


uint8_t *flash_buffer;

#define SECTOR_SIZE 512

#define FLASH_PART1_START_BLOCK (0x100)
#define FLASH_PART1_NUM_BLOCKS (_BUFFER_SIZE_KiB*2)
#define FLASH_ROW_SIZE 128

FATFS fs;

PARTITION VolToPart[]={
    {0,1},
    {0,1},
    {0,1},
    {0,1},
};

static void build_partition(uint8_t *buf, int boot, int type, uint32_t start_block, uint32_t num_blocks) {
  buf[0] = boot;

  if (num_blocks == 0) {
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
  } else {
    buf[1] = 0xff;
    buf[2] = 0xff;
    buf[3] = 0xff;
  }

  buf[4] = type;

  if (num_blocks == 0) {
    buf[5] = 0;
    buf[6] = 0;
    buf[7] = 0;
  } else {
    buf[5] = 0xff;
    buf[6] = 0xff;
    buf[7] = 0xff;
  }

  buf[8] = start_block;
  buf[9] = start_block >> 8;
  buf[10] = start_block >> 16;
  buf[11] = start_block >> 24;

  buf[12] = num_blocks;
  buf[13] = num_blocks >> 8;
  buf[14] = num_blocks >> 16;
  buf[15] = num_blocks >> 24;
}

void sector_dump(void){
  FILE *fp;
  int i;
  fp = fopen("fat.csv","w");
  for(i=0;i<_BUFFER_SIZE_KiB*1024;i++){
    fprintf(fp,"0x%02x,",flash_buffer[i]);
    if(i%16==15){
      fprintf(fp,"\n");
    }
  }
  fclose(fp);
}

int sector_read(const unsigned int page,const unsigned int number_of_sector,uint8_t *buffer){
  if(page == 0){
    memset(buffer,0,446);
    build_partition(buffer + 446, 0, 0x01 /* FAT12 */, FLASH_PART1_START_BLOCK, FLASH_PART1_NUM_BLOCKS);
    build_partition(buffer + 462, 0, 0, 0, 0);/* empty */
    build_partition(buffer + 478, 0, 0, 0, 0);
    build_partition(buffer + 494, 0, 0, 0, 0);
    buffer[510] = 0x55;
    buffer[511] = 0xaa;

  } else if (page-FLASH_PART1_START_BLOCK < _BUFFER_SIZE_KiB*2){
    memcpy(buffer,flash_buffer+(page-FLASH_PART1_START_BLOCK)*SECTOR_SIZE,number_of_sector*SECTOR_SIZE);
    return 1;
  } else {
    return 0;
  }
  return 0;
}


int write1page(void *buff,void *flashaddr){
  const void *b = buff;
  do{
    memcpy(flashaddr, b, 128);
    b += 128,flashaddr += 128;
  }while(b!=buff+1024);
  return 0;
}

void sector_write(const unsigned int page,unsigned int number_of_sector,const uint8_t *buffer){

  if(page == 0||number_of_sector == 0){
    return ;
  } else if (page-FLASH_PART1_START_BLOCK < _BUFFER_SIZE_KiB*2){
    void* flash_addr;
    flash_addr = (page-FLASH_PART1_START_BLOCK) * 512 + flash_buffer;
    memcpy(flash_addr,buffer,number_of_sector*512);
  }
}

DRESULT disk_ioctl (
	uint8_t drv,		/* Physical drive number (0) */
	uint8_t ctrl,		/* Control code */
	void    *buff		/* Buffer to send/receive control data */
)
{
    if(ctrl == GET_SECTOR_COUNT){
        *(DWORD*)buff = _BUFFER_SIZE_KiB*2;
    }else if(ctrl == GET_SECTOR_SIZE){
        *(DWORD*)buff = 512;
    }else if(ctrl == GET_BLOCK_SIZE){
        *(DWORD*)buff = 1;
    }
    return 0;
}

FRESULT init_flashfs(void){
  FRESULT res;
  FIL fil;
  flash_buffer = calloc(1024,_BUFFER_SIZE_KiB);
  res = f_mount(&fs, "", 0);
  res = f_open(&fil,"",FA_READ);
  printf("fatfs initializing...\n");

  if(res == FR_NO_FILESYSTEM){
    printf("creating fat...\n");
    res = f_mkfs("",0,4);
    if(res)return res;
    res = f_mount(&fs,"",0);
  }
  if(res)return res;
  res = f_mount(&fs,"",0);
  if(res)return res;
  printf("success\n");
  return res;
}

DSTATUS disk_status(BYTE pdrv){
    return 0;
}

DRESULT disk_read (BYTE drv, BYTE* buff, DWORD sector, UINT count){
    sector_read(sector,count,buff);
    return 0;
}

DRESULT disk_write (BYTE drv,const BYTE* buff, DWORD sector, UINT count){
    sector_write(sector,count,buff);
    return 0;
}


DSTATUS disk_initialize (BYTE pdrv){
    return 0;
}
