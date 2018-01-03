#ifndef __FLASH_H
#define __FLASH_H

#include "ff.h"

FRESULT init_flashfs(void);
void sector_dump(void);

#endif
