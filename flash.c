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

#include "ff.h"
#include "diskio.h"
#include <string.h>
#include "NVMem.h"

#define _BUFFER_SIZE_KiB 80

const uint8_t flash_buffer[1024*_BUFFER_SIZE_KiB] __attribute__((aligned(1024)))={
#include "fat.csv"
};

#define SECTOR_SIZE 512

#define FLASH_PART1_START_BLOCK (0x100)
#define FLASH_PART1_NUM_BLOCKS (_BUFFER_SIZE_KiB*2)
#define FLASH_ROW_SIZE 128

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

static
int sector_read(const unsigned int page,const unsigned int number_of_sector,uint8_t *buffer){
  if(page == 0){
    memset(buffer,0,446);
    build_partition(buffer + 446, 0, 0x01 /* FAT12 */, FLASH_PART1_START_BLOCK, FLASH_PART1_NUM_BLOCKS);
    build_partition(buffer + 462, 0, 0, 0, 0);/* empty */
    build_partition(buffer + 478, 0, 0, 0, 0);
    build_partition(buffer + 494, 0, 0, 0, 0);
    buffer[510] = 0x55;
    buffer[511] = 0xaa;
    return 1;
  } else if (page-FLASH_PART1_START_BLOCK < _BUFFER_SIZE_KiB*2){
    memcpy(buffer,flash_buffer+(page-FLASH_PART1_START_BLOCK)*SECTOR_SIZE,number_of_sector*SECTOR_SIZE);
    return 1;
  } else {
    return 0;
  }
}


static
int write1page(const void *buff,const void *flashaddr){
  int ret;
  const void *b = buff;
  do{
    ret = NVMemWriteRow(flashaddr,b);
    b += 128,flashaddr += 128;
  }while(ret==0&&b!=buff+1024);
  return 0;
}

static
int sector_write(const unsigned int page,unsigned int number_of_sector,const uint8_t *buffer){
  int ret;
  uint8_t flash_temp_buffer[1024] __attribute__((aligned(4)));

  if(page == 0||number_of_sector == 0){
    return 0;
  } else if (page-FLASH_PART1_START_BLOCK < _BUFFER_SIZE_KiB*2){
    const void* flash_addr;
    flash_addr = (page-FLASH_PART1_START_BLOCK) * 512 + flash_buffer;
    if((unsigned int)flash_addr & (1<<9)){//sector alignment required!
      memcpy(flash_temp_buffer,flash_addr-SECTOR_SIZE,SECTOR_SIZE);
      memcpy(flash_temp_buffer+SECTOR_SIZE,buffer,SECTOR_SIZE);
      ret = NVMemErasePage(flash_addr-SECTOR_SIZE);
      if(ret)return ret;
      ret = write1page(flash_temp_buffer,flash_addr-SECTOR_SIZE);
      if(ret)return ret;
      if(number_of_sector!=1)sector_write(page+1, number_of_sector-1, buffer+SECTOR_SIZE);
    }else if(number_of_sector == 1){//last write
      memcpy(flash_temp_buffer,buffer,SECTOR_SIZE);
      memcpy(flash_temp_buffer+SECTOR_SIZE,flash_addr+SECTOR_SIZE,SECTOR_SIZE);
      ret = NVMemErasePage(flash_addr);
      if(ret)return ret;
      ret = write1page(flash_temp_buffer,flash_addr);
      if(ret)return ret;
    }else{//2sectors aligned
      memcpy(flash_temp_buffer,buffer,SECTOR_SIZE*2);
      ret = NVMemErasePage(flash_addr);
      if(ret)return ret;
      ret = write1page(flash_temp_buffer,flash_addr);
      if(ret)return ret;
      if(number_of_sector!=1)sector_write(page+2, number_of_sector-2, buffer+SECTOR_SIZE*2);
    }
  }
  return 0;
}

DRESULT disk_ioctl_flash (
	uint8_t ctrl,		/* Control code */
	void    *buff		/* Buffer to send/receive control data */
)
{
    if(ctrl == GET_SECTOR_COUNT){
        *(DWORD*)buff = _BUFFER_SIZE_KiB*2;
    }else if(ctrl == GET_SECTOR_SIZE){
        *(DWORD*)buff = 512;
    }else if(ctrl == GET_BLOCK_SIZE){
        *(DWORD*)buff = 2;
    }
    return 0;
}

DSTATUS disk_status_flash(void){
    return 0;
}

DRESULT disk_read_flash ( BYTE* buff, DWORD sector, UINT count){
    sector_read(sector,count,buff);
    return 0;
}

DRESULT disk_write_flash (const BYTE* buff, DWORD sector, UINT count){
    sector_write(sector,count,buff);
    return 0;
}

DSTATUS disk_initialize_flash (void){
    return 0;
}
