/********************************************************************************/
/*!
	@file			mmc_xmega.h
	@author         Nemui Trinomius (http://nemuisan.blog.bai.ne.jp)
    @version        6.00
    @date           2014.01.20
	@brief          MMC Driver For ATMEL XMEGA  Devices					@n
					Based on ChaN's FatFs Sample thanks!				@n
					This Header is MMC Driver's HAL Configure!

    @section HISTORY
		2011.11.20	V1.00 	Start Here.
		2012.05.26	V2.00 	Added DMA Transaction.
		2012.09.30  V3.00 	Updated Support Grater Than 32GB Cards.
		2013.10.16	V4.00	Adopted FatFs0.10.
		2014.01.20	V5.00	Adopted FatFs0.10a.
		2015.02.01	V6.00	Improve Portability.

    @section LICENSE
		BSD License. See Copyright-SD.txt
*/
/********************************************************************************/
#ifndef __MMC_XMEGA_H
#define __MMC_XMEGA_H	0x0600

#define _READONLY 0
#include <stdint.h>

#define SPI_START_CFG_1     (PRI_PRESCAL_64_1 | SEC_PRESCAL_8_1 | MASTER_ENABLE_ON | SPI_CKE_ON | SPI_SMP_ON)
#define SPI_START_CFG_2     (SPI_ENABLE)

// Description: SD-SPI Chip Select Output bit
#define SD_CS               LATAbits.LATA0
// Description: SD-SPI Chip Select TRIS bit
#define SD_CS_TRIS          TRISAbits.TRISA0

// Description: The main SPI control register
#define SPICON1             SPI2CON
// Description: The SPI status register
#define SPISTAT             SPI2STAT
// Description: The SPI Buffer
#define SPIBUF              SPI2BUF
// Description: The receive buffer full bit in the SPI status register
#define SPISTAT_RBF         SPI2STATbits.SPIRBF
// Description: The bitwise define for the SPI control register (i.e. _____bits)
#define SPICON1bits         SPI2CONbits
// Description: The bitwise define for the SPI status register (i.e. _____bits)
#define SPISTATbits         SPI2STATbits
// Description: The enable bit for the SPI module
#define SPIENABLE           SPI2CONbits.ON
// Description: The definition for the SPI baud rate generator register (PIC32)
#define SPIBRG			    SPI2BRG

// Tris pins for SCK/SDI/SDO lines

// Description: The TRIS bit for the SCK pin
#define SPICLOCK            TRISBbits.TRISB15
// Description: The TRIS bit for the SDI pin
#define SPIIN               TRISAbits.TRISA4
// Description: The TRIS bit for the SDO pin
#define SPIOUT              TRISBbits.TRISB5
//SPI library functions
#define putcSPI             putcSPI2
#define getcSPI             getcSPI2
#define OpenSPI(config1, config2)   OpenSPI2(config1, config2)


/* Port Controls  (Platform dependent) */
#define MSPIPORT	SPI2
#define MSPIGPIO	PORTD
#define MSPI_MOSI	PIN7_bm
#define MSPI_MISO   PIN6_bm
#define MSPI_XCK    PIN5_bm
#define MSPI_CS    	PIN4_bm

#define MSPI_Tx_Enable(_mspi)	USART_Tx_Enable(_mspi)
#define MSPI_Tx_Disable(_mspi)  USART_Tx_Disable(_mspi)

#define MSPI_Rx_Enable(_mspi)	USART_Rx_Enable(_mspi)
#define MSPI_Rx_Disable(_mspi)  USART_Rx_Disable(_mspi)

#define MSPI_SetMode(_mspi, _bselValue, _spiMode) \
						(_mspi)->CTRLC = (USART_CMODE_MSPI_gc | _spiMode); \
						(_mspi)->BAUDCTRLA =(uint8_t)_bselValue
						
#define MSPI_ClrIntFLag(_mspi) \
						(_mspi)->STATUS = 0

#define MSPI_SendByte(_mspi, _data) \
						USART_PutChar(_mspi, _data)

#define MSPI_GetByte(_mspi) \
						USART_GetChar(_mspi)

#define MSPI_Rx_Completed(_mspi) \
						USART_IsRXComplete(_mspi)
						
#define MSPI_IsTXDataRegisterEmpty(_mspi) \
						USART_IsTXDataRegisterEmpty(_mspi)


/* MSPI DMA Settings */
/* If U want DMAed FatFs,Uncomment Below! */
#define USE_SPIMMC_DMA

#define FF_DMA_RX		DMA.CH0
#define FF_DMA_TX		DMA.CH1
#define RXCH_TRNIF		DMA_CH0TRNIF_bm
#define TXCH_TRNIF		DMA_CH1TRNIF_bm
#define TXRX_ERRIF		(DMA_CH0ERRIF_bm | DMA_CH1ERRIF_bm)
#define DMA_TRIGSRC_RXC	DMA_CH_TRIGSRC_USARTD1_RXC_gc
#define DMA_TRIGSRC_DRE	DMA_CH_TRIGSRC_USARTD1_DRE_gc

/* Port Controls */
#define CS_LOW()		MSPIGPIO.OUTCLR = MSPI_CS;	/* MMC CS = L */
#define	CS_HIGH()		MSPIGPIO.OUTSET = MSPI_CS;	/* MMC CS = H */


#define SOCKPORT		MSPIGPIO.IN		/* Socket contact port */
#define SOCKWP			0				/* Write protect switch	0: WP Disable */
#define SOCKINS			1				/* Card detect switch	1: SD Incerted */

#define	FCLK_SLOW()		SPIBRG=100 /* Set slow clock (100k-400k) */
#define	FCLK_FAST()		SPIBRG=0    /* Set fast clock (depends on the CSD) */

#define PWR_ON()		
#define PWR_OFF()		
#define PWR_SET()
#define PWR_ISON()		1			/* Always 1 */ 

extern void disk_timerproc(void);

#endif /* _MMC_XMEGA_IF */
