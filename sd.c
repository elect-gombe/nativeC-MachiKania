/********************************************************************************/
/*!
	@file			mmc_xmega.c
	@author         Nemui Trinomius (http://nemuisan.blog.bai.ne.jp)
    @version        6.00
    @date           2014.01.20
	@brief          MMC Driver For ATMEL XMEGA  Devices					@n
					Based on ChaN's FatFs Sample thanks!

    @section HISTORY
		2011.11.20	V1.00 	Start Here.
		2012.05.26	V2.00 	Added DMA Transaction.
		2012.09.30  V3.00 	Updated Support Grater Than 32GB Cards.
		2013.10.16	V4.00	Adopted FatFs0.10.
		2014.01.20	V5.00	Adopted FatFs0.10a.
		2015.02.01	V6.00	Improve Portability.

    @section LICENSE
		BSD License. See Copyright.txt
*/
/********************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "mmc_xmega.h"
#include "diskio.h"
#include <xc.h>
#define _SUPPRESS_PLIB_WARNING
#include <plib.h>
/* check header file version for fool proof */
#if __MMC_XMEGA_H!= 0x0600
#error "header file version is not correspond!"
#endif
#include "ff.h"

FATFS sdfs;

int init_flashfs(void){
  f_mount(&sdfs,"",0);
}

/* Defines -------------------------------------------------------------------*/
/* Definitions for MMC/SDC command */
#define CMD0	(0)					/* GO_IDLE_STATE */
#define CMD1	(1)					/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)			/* SEND_OP_COND (SDC) */
#define CMD8	(8)					/* SEND_IF_COND */
#define CMD9	(9)					/* SEND_CSD */
#define CMD10	(10)				/* SEND_CID */
#define CMD12	(12)				/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)			/* SD_STATUS (SDC) */
#define CMD16	(16)				/* SET_BLOCKLEN */
#define CMD17	(17)				/* READ_SINGLE_BLOCK */
#define CMD18	(18)				/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)				/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)			/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)				/* WRITE_BLOCK */
#define CMD25	(25)				/* WRITE_MULTIPLE_BLOCK */
#define CMD55	(55)				/* APP_CMD */
#define CMD58	(58)				/* READ_OCR */

uint8_t dumydata[512] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,    
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,    
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,    
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,    
};

/* Variables -----------------------------------------------------------------*/
static volatile DSTATUS Stat = STA_NOINIT;		/* Disk status */
static volatile unsigned int Timer1, Timer2;	/* 100Hz decrement timer */
static uint8_t CardType;						/* Card type flags */

/* Constants -----------------------------------------------------------------*/

/* Function prototypes -------------------------------------------------------*/

/* Functions -----------------------------------------------------------------*/

#define xmit_spi(dat)	\
    putcSPI(dat);\
    getcSPI()

/**************************************************************************/
/*! 
    @brief  Receive a byte from MMC via SPI.
	@param  none
    @retval : uint8_t
*/
/**************************************************************************/

uint8_t rcvr_spi(void){
    putcSPI(0xFF);
    return getcSPI();
}

uint8_t rcvr_spi_m(uint8_t *dst){
    putcSPI(0xFF);
    *dst = getcSPI();
}


/**************************************************************************/
/*! 
    @brief  Wait for card ready.
	@param  none
    @retval : 1:OK, 0:Timeout
*/
/**************************************************************************/
static int wait_ready (	/* 1:Ready, 0:Timeout */
	unsigned int wt		/* Timeout [ms/10] */
)
{
	Timer2 = wt;
	rcvr_spi();
	do
		if (rcvr_spi() == 0xFF) return 1;
	while (Timer2);

	return 0;
}

/**************************************************************************/
/*! 
    @brief  Deselect the card and release SPI bus.
	@param  none
    @retval : none
*/
/**************************************************************************/
static inline void deselect(void)
{
	SD_CS = 1;
	rcvr_spi();		/* Dummy clock (force DO hi-z for multiple slave SPI) */
}

/**************************************************************************/
/*! 
    @brief  Select the card and wait for ready.
	@param  none
    @retval : 1:Successful, 0:Timeout
*/
/**************************************************************************/
static inline int select(void)
{
    SD_CS = 0;
	rcvr_spi();		/* Dummy clock (force DO hi-z for multiple slave SPI) */

	if (wait_ready(50)) return 1;	/* OK */
	deselect();
	return 0;	/* Timeout */
}

/**************************************************************************/
/*! 
    @brief  Power Control.												  @m
			When the target system does not support socket power control, @n
			thereis nothing to do in these functions and chk_power always @n
			returns 1.
	@param  none
    @retval : Socket power state: 0=off, 1=on
*/
/**************************************************************************/
static int power_status(void)
{
	return PWR_ISON() ? 1 : 0;
}

/**************************************************************************/
/*! 
    @brief  Power Control.
	@param  none
    @retval : none
*/
/**************************************************************************/
static void power_on (void)
{
	PWR_SET();				/* Power control Set */
	PWR_ON();				/* Socket power ON */
	
//	for (Timer1 = 25; Timer1; );	/* Wait for 250ms */

    // Turn off the card
    SD_CS_TRIS = 0;           //Card Select - output
    SD_CS = 1;                     //Initialize Chip Select line
    
    SPICLOCK = 0;
    SPIIN = 1;
    SPIOUT = 0;

	OpenSPI(SPI_START_CFG_1, SPI_START_CFG_2);
    
    SPIBRG = 100;
}

/**************************************************************************/
/*! 
    @brief  Power Control.
	@param  none
    @retval : none
*/
/**************************************************************************/
static void power_off (void)
{
	if (!(Stat & STA_NOINIT)) {
		SD_CS=1;
		wait_ready(50);
		deselect();
	}
	PWR_OFF();
	
	Stat |= STA_NOINIT;		/* Set STA_NOINIT */
}

/**************************************************************************/
/*! 
    @brief  Receive a data packet from MMC.
	@param  none
    @retval : none
*/
/**************************************************************************/
static int rcvr_datablock (
	uint8_t 		*buff,			/* Data buffer to store received data */
	unsigned int 	 btr			/* Byte count (must be multiple of 4) */
)
{
	uint8_t token;


	Timer1 = 20;
	do {							/* Wait for data packet in timeout of 200ms */
		token = rcvr_spi();
	} while ((token == 0xFF) && Timer1);
	if(token != 0xFE) return 0;		/* If not valid data token, retutn with error */

#ifdef USE_SPIMMC_DMA
    DmaChnOpen(1, DMA_CHN_PRI2, DMA_OPEN_DEFAULT);
    DmaChnClrEvFlags(1,DMA_EV_ALL_EVNTS);
    DmaChnSetEventControl(1, DMA_EV_START_IRQ_EN|DMA_EV_START_IRQ(_SPI2_RX_IRQ));
    DmaChnSetTxfer(1, dumydata, (void*) &SPI2BUF, btr, 1, 1);

    DmaChnOpen(2, DMA_CHN_PRI2, DMA_OPEN_DEFAULT);
    DmaChnClrEvFlags(2,DMA_EV_ALL_EVNTS);
    DmaChnSetEventControl(2, DMA_EV_START_IRQ_EN|DMA_EV_START_IRQ(_SPI2_RX_IRQ));
    DmaChnSetTxfer(2, (void*) &SPI2BUF,buff, 1, btr, 1);

    DmaChnEnable(2);
    DmaChnStartTxfer(1, DMA_WAIT_NOT, 0);

    while(!(DmaChnGetEvFlags(2) & DMA_EV_DST_FULL));
#else
	do {							/* Receive the data block into buffer */
		rcvr_spi_m(buff++);
		rcvr_spi_m(buff++);
		rcvr_spi_m(buff++);
		rcvr_spi_m(buff++);
	} while (btr -= 4);
#endif /* USE_SPIMMC_DMA */
	rcvr_spi();						/* Discard CRC */
	rcvr_spi();
	
	return 1;						/* Return with success */
}

/**************************************************************************/
/*! 
    @brief  Send a data packet to MMC.
	@param  none
    @retval : none
*/
/**************************************************************************/
static int xmit_datablock (
	const uint8_t *buff,	/* 512 byte data block to be transmitted */
	uint8_t       token		/* Data/Stop token */
)
{
	uint8_t resp;
#ifndef USE_SPIMMC_DMA
	uint8_t wc;
#endif

	if (!wait_ready(50)) return 0;

	xmit_spi(token);					/* Xmit data token */
	if (token != 0xFD) {				/* Is data token */
#ifdef USE_SPIMMC_DMA
	/* dma_send_transfer(buff, 512); */
#else
		wc = 0;
		do {							/* Xmit the 512 byte data block to MMC */
			xmit_spi(*buff++);
			xmit_spi(*buff++);
		} while (--wc);
#endif /* USE_SPIMMC_DMA */
		xmit_spi(0xFF);					/* CRC (Dummy) */
		xmit_spi(0xFF);
		resp = rcvr_spi();				/* Reveive data response */
		if ((resp & 0x1F) != 0x05)		/* If not accepted, return with error */
			return 0;
	}

	return 1;
}

/**************************************************************************/
/*! 
    @brief  Send a command packet to MMC .
	@param  none
    @retval : none
*/
/**************************************************************************/
static uint8_t send_cmd (		/* Returns R1 resp (bit7==1:Send failed) */
	uint8_t  cmd,				/* Command index */
	uint32_t arg				/* Argument */
)
{
	uint8_t n, res;


	if (cmd & 0x80) {	/* ACMD<n> is the command sequense of CMD55-CMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect();
		if (!select()) return 0xFF;
	}

	/* Send command packet */
	xmit_spi(0x40 | cmd);				/* Start + Command index */
	xmit_spi((uint8_t)(arg >> 24));		/* Argument[31..24] */
	xmit_spi((uint8_t)(arg >> 16));		/* Argument[23..16] */
	xmit_spi((uint8_t)(arg >> 8));			/* Argument[15..8] */
	xmit_spi((uint8_t)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	xmit_spi(n);

	/* Receive command response */
	if (cmd == CMD12) rcvr_spi();		/* Skip a stuff byte when stop reading */
	n = 10;								/* Wait for a valid response in timeout of 10 attempts */
	do
		res = rcvr_spi();
	while ((res & 0x80) && --n);

	return res;			/* Return with the response value */
}





/**************************************************************************/
/*! 
    Public Functions
*/
/**************************************************************************/
/**************************************************************************/
/*! 
    @brief  Initialize Disk Drive.
	@param  none
    @retval : none
*/
/**************************************************************************/
DSTATUS disk_initialize_sd (void)
{
	uint8_t n, cmd, ty, ocr[4];


	if (Stat & STA_NODISK) return Stat;	/* No card in the socket */

	power_on();							/* Force socket power on */
	FCLK_SLOW();
	for (n = 10; n; n--) rcvr_spi();	/* 80 dummy clocks */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
		Timer1 = 100;						/* Initialization timeout of 1000 msec */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2? */
			for (n = 0; n < 4; n++) ocr[n] = rcvr_spi();		/* Get trailing return value of R7 resp */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {				/* The card can work at vdd range of 2.7-3.6V */
				while (Timer1 && send_cmd(ACMD41, 1UL << 30));	/* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (Timer1 && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = rcvr_spi();
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 */
				}
			}
		} else {							/* SDv1 or MMCv3 */
			if (send_cmd(ACMD41, 0) <= 1) 	{
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			while (Timer1 && send_cmd(cmd, 0));			/* Wait for leaving idle state */
			if (!Timer1 || send_cmd(CMD16, 512) != 0)	/* Set R/W block length to 512 */
				ty = 0;
		}
	}
	CardType = ty;
	deselect();

	if (ty) {						/* Initialization succeded */
		Stat &= ~STA_NOINIT;		/* Clear STA_NOINIT */

		FCLK_FAST();

	} else {						/* Initialization failed */
		power_off();
	}

	return Stat;
}

/**************************************************************************/
/*! 
    @brief  Get Disk Status.
	@param  none
    @retval : none
*/
/**************************************************************************/
DSTATUS disk_status_sd (void)
{
	return Stat;
}

/**************************************************************************/
/*! 
    @brief  Read Sector(s).
	@param  none
    @retval : none
*/
/**************************************************************************/
DRESULT disk_read_sd ( BYTE* buff, DWORD sector, UINT count){
	if (Stat & STA_NOINIT) return RES_NOTRDY;

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* Convert to byte address if needed */

	if (count == 1) {	/* Single block read */
		if ((send_cmd(CMD17, sector) == 0)	/* READ_SINGLE_BLOCK */
			&& rcvr_datablock(buff, 512))
			count = 0;
	}
	else {				/* Multiple block read */
		if (send_cmd(CMD18, sector) == 0) {	/* READ_MULTIPLE_BLOCK */
			do {
				if (!rcvr_datablock(buff, 512)) break;
				buff += 512;
			} while (--count);
			send_cmd(CMD12, 0);				/* STOP_TRANSMISSION */
		}
	}
	deselect();

	return count ? RES_ERROR : RES_OK;
}

/**************************************************************************/
/*! 
    @brief  Write Sector(s).
	@param  none
    @retval : none
*/
/**************************************************************************/
DRESULT disk_write_sd (
	const    uint8_t *buff,	/* Pointer to the data to be written */
	uint32_t sector,		/* Start sector number (LBA) */
	unsigned int count		/* Sector count */
)
{
	if (Stat & STA_NOINIT) return RES_NOTRDY;
	if (Stat & STA_PROTECT) return RES_WRPRT;

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* Convert to byte address if needed */

	if (count == 1) {	/* Single block write */
		if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE))
			count = 0;
	}
	else {				/* Multiple block write */
		if (CardType & CT_SDC) send_cmd(ACMD23, count);
		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD))	/* STOP_TRAN token */
				count = 1;
		}
	}
	deselect();

	return count ? RES_ERROR : RES_OK;
}

/**************************************************************************/
/*! 
    @brief  Miscellaneous Functions.
	@param  none
    @retval : none
*/
/**************************************************************************/
DRESULT disk_ioctl_sd (
	uint8_t ctrl,		/* Control code */
	void    *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	uint8_t n, csd[16], *ptr = buff;
	uint32_t *dp, st, ed, csize;

	res = RES_ERROR;

	if (ctrl == CTRL_POWER_OFF) {
		switch (ptr[0]) {
		case 0:		/* Sub control code (POWER_OFF) */
			power_off();		/* Power off */
			res = RES_OK;
			break;
		case 1:		/* Sub control code (POWER_GET) */
			ptr[1] = (uint8_t)power_status();
			res = RES_OK;
			break;
		default :
			res = RES_PARERR;
		}
	}
	else {
		if (Stat & STA_NOINIT) return RES_NOTRDY;

		switch (ctrl) {
		case CTRL_SYNC :		/* Make sure that no pending write process. Do not remove this or written sector might not left updated. */
			if (select()) {
				deselect();
				res = RES_OK;
			}
			break;

		case GET_SECTOR_COUNT :	/* Get drive capacity in unit of sector (uint32_t) */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
				if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
					csize = csd[9] + ((uint16_t)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
					*(uint32_t*)buff = csize << 10;
				} else {					/* SDC ver 1.XX or MMC ver 3 */
					n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
					csize = (csd[8] >> 6) + ((uint16_t)csd[7] << 2) + ((uint16_t)(csd[6] & 3) << 10) + 1;
					*(uint32_t*)buff = csize << (n - 9);
				}
				res = RES_OK;
			}
			break;

		case GET_SECTOR_SIZE :	/* Get R/W sector size (uint16_t) */
			*(uint16_t*)buff = 512;
			res = RES_OK;
			break;

		case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (uint32_t) */
			if (CardType & CT_SD2) {	/* SDv2? */
				if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
					rcvr_spi();
					if (rcvr_datablock(csd, 16)) {				/* Read partial block */
						for (n = 64 - 16; n; n--) rcvr_spi();	/* Purge trailing data */
						*(uint32_t*)buff = 16UL << (csd[10] >> 4);
						res = RES_OK;
					}
				}
			} else {					/* SDv1 or MMCv3 */
				if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
					if (CardType & CT_SD1) {	/* SDv1 */
						*(uint32_t*)buff = (((csd[10] & 63) << 1) + ((uint16_t)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
					} else {					/* MMCv3 */
						*(uint32_t*)buff = ((uint16_t)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
					}
					res = RES_OK;
				}
			}
			break;

		case MMC_GET_TYPE :		/* Get card type flags (1 byte) */
			*ptr = CardType;
			res = RES_OK;
			break;

		case MMC_GET_CSD :		/* Receive CSD as a data block (16 bytes) */
			if (send_cmd(CMD9, 0) == 0		/* READ_CSD */
				&& rcvr_datablock(ptr, 16))
				res = RES_OK;
			break;

		case MMC_GET_CID :		/* Receive CID as a data block (16 bytes) */
			if (send_cmd(CMD10, 0) == 0		/* READ_CID */
				&& rcvr_datablock(ptr, 16))
				res = RES_OK;
			break;

		case MMC_GET_OCR :		/* Receive OCR as an R3 resp (4 bytes) */
			if (send_cmd(CMD58, 0) == 0) {	/* READ_OCR */
				for (n = 4; n; n--) *ptr++ = rcvr_spi();
				res = RES_OK;
			}
			break;

		case MMC_GET_SDSTAT :	/* Receive SD statsu as a data block (64 bytes) */
			if (send_cmd(ACMD13, 0) == 0) {	/* SD_STATUS */
				rcvr_spi();
				if (rcvr_datablock(ptr, 64))
					res = RES_OK;
			}
			break;

		default:
			res = RES_PARERR;
		}

		deselect();
	}

	return res;
}


/* End Of File ---------------------------------------------------------------*/
