#ifndef _XENON_SFC_H
#define _XENON_SFC_H

#include <stdbool.h>

//Registers
#define SFCX_CONFIG				(0x00)
#define SFCX_STATUS 			(0x04)
#define SFCX_COMMAND			(0x08)
#define SFCX_ADDRESS			(0x0C)
#define SFCX_DATA				(0x10)
#define SFCX_LOGICAL 			(0x14)
#define SFCX_PHYSICAL			(0x18)
#define SFCX_DATAPHYADDR		(0x1C)
#define SFCX_SPAREPHYADDR		(0x20)
#define SFCX_MMC_IDENT			(0xFC)

//Commands for Command Register
#define PAGE_BUF_TO_REG			(0x00) 			//Read page buffer to data register
#define REG_TO_PAGE_BUF			(0x01) 			//Write data register to page buffer
#define LOG_PAGE_TO_BUF			(0x02) 			//Read logical page into page buffer
#define PHY_PAGE_TO_BUF			(0x03) 			//Read physical page into page buffer
#define WRITE_PAGE_TO_PHY		(0x04) 			//Write page buffer to physical page
#define BLOCK_ERASE				(0x05) 			//Block Erase
#define DMA_LOG_TO_RAM			(0x06) 			//DMA logical flash to main memory
#define DMA_PHY_TO_RAM			(0x07) 			//DMA physical flash to main memory
#define DMA_RAM_TO_PHY			(0x08) 			//DMA main memory to physical flash
#define UNLOCK_CMD_0			(0x55) 			//Unlock command 0
#define UNLOCK_CMD_1			(0xAA) 			//Unlock command 1

//Config Register bitmasks
#define CONFIG_DBG_MUX_SEL  	(0x7C000000)	//Debug MUX Select
#define CONFIG_DIS_EXT_ER   	(0x2000000)		//Disable External Error (Pre Jasper?)
#define CONFIG_CSR_DLY      	(0x1FE0000)		//Chip Select to Timing Delay
#define CONFIG_ULT_DLY      	(0x1F800)		//Unlocking Timing Delay
#define CONFIG_BYPASS       	(0x400)			//Debug Bypass
#define CONFIG_DMA_LEN      	(0x3C0)			//DMA Length in Pages
#define CONFIG_FLSH_SIZE    	(0x30)			//Flash Size (Pre Jasper)
#define CONFIG_WP_EN        	(0x8)			//Write Protect Enable
#define CONFIG_INT_EN       	(0x4)			//Interrupt Enable
#define CONFIG_ECC_DIS      	(0x2)			//ECC Decode Disable : TODO: make sure this isn't disabled before reads and writes!!
#define CONFIG_SW_RST       	(0x1)			//Software reset

#define CONFIG_DMA_PAGES(x)		(((x-1)<<6)&CONFIG_DMA_LEN)

//Status Register bitmasks
#define STATUS_MASTER_ABOR		(0x4000)		// DMA master aborted if not zero
#define STATUS_TARGET_ABOR		(0x2000)		// DMA target aborted if not zero
#define STATUS_ILL_LOG      	(0x800)			//Illegal Logical Access
#define STATUS_PIN_WP_N     	(0x400)			//NAND Not Write Protected
#define STATUS_PIN_BY_N     	(0x200)			//NAND Not Busy
#define STATUS_INT_CP       	(0x100)			//Interrupt
#define STATUS_ADDR_ER      	(0x80)			//Address Alignment Error
#define STATUS_BB_ER        	(0x40)			//Bad Block Error
#define STATUS_RNP_ER       	(0x20)			//Logical Replacement not found
#define STATUS_ECC_ER       	(0x1C)			//ECC Error, 3 bits, need to determine each
#define STATUS_WR_ER        	(0x2)			//Write or Erase Error
#define STATUS_BUSY         	(0x1)			//Busy
#define STATUS_ECC_ERROR		(0x10)			// controller signals unrecoverable ECC error when (!((stat&0x1c) < 0x10))
#define STATUS_DMA_ERROR		(STATUS_MASTER_ABOR|STATUS_TARGET_ABOR)
#define STATUS_ERROR			(STATUS_ILL_LOG|STATUS_ADDR_ER|STATUS_BB_ER|STATUS_RNP_ER|STATUS_ECC_ERROR|STATUS_WR_ER|STATUS_MASTER_ABOR|STATUS_TARGET_ABOR)
#define STSCHK_WRIERA_ERR(sta)	((sta & STATUS_WR_ER) != 0)
#define STSCHK_ECC_ERR(sta)		(!((sta & STATUS_ECC_ER) < 0x10))
#define STSCHK_DMA_ERR(sta)		((sta & (STATUS_DMA_ERROR) != 0)

//Page bitmasks
#define PAGE_VALID          	(0x4000000)
#define PAGE_PID            	(0x3FFFE00)

// status ok or status ecc corrected
//#define SFCX_SUCCESS(status) (((int) status == STATUS_PIN_BY_N) || ((int) status & STATUS_ECC_ER))
// define success as no ecc error and no bad block error
#define SFCX_SUCCESS(status) ((status&STATUS_ERROR)==0)

#define MAX_PAGE_SZ 			0x210			//Max known hardware physical page size
#define MAX_BLOCK_SZ 			0x42000 		//Max known hardware physical block size

#define META_TYPE_SM				0x00 			//Pre Jasper - Small Block
#define META_TYPE_BOS				0x01 			//Jasper 16MB - Big Block on Small NAND
#define META_TYPE_BG				0x02			//Jasper 256MB and 512MB Big Block
#define META_TYPE_NONE				0x04				//eMMC doesn't have Spare Data

#define CONFIG_BLOCKS			0x04			//Number of blocks assigned for config data

#define SFCX_INITIALIZED		1

#define INVALID					-1

// status ok or status ecc corrected
//#define SFCX_SUCCESS(status) (((int) status == STATUS_PIN_BY_N) || ((int) status & STATUS_ECC_ER))
// define success as no ecc error and no bad block error
#define SFCX_SUCCESS(status) ((status&STATUS_ERROR)==0)

typedef struct _xenon_nand
{
	bool init;
	bool isBBCont;
	bool isBB;
	bool MMC;
	unsigned char MetaType;

	unsigned short PageSz;
	unsigned short PageSzPhys;
	unsigned char MetaSz;

	unsigned short PagesInBlock;
	unsigned int BlockSz;
	unsigned int BlockSzPhys;

	unsigned int PagesCount;
	unsigned int BlocksCount;

	unsigned int SizeSpare;
	unsigned int SizeData;
	unsigned int SizeDump;
	unsigned int SizeWrite;

	unsigned short SizeUsableFs;
	unsigned short ConfigBlock;
} xenon_nand, *pxenon_nand;

unsigned long xenon_sfc_ReadReg(unsigned int addr);
void xenon_sfc_WriteReg(unsigned int addr, unsigned long data);

int xenon_sfc_ReadPagePhy(unsigned char* buf, unsigned int page);
int xenon_sfc_ReadPageLog(unsigned char* buf, unsigned int page);
int xenon_sfc_WritePage(unsigned char* buf, unsigned int page);
int xenon_sfc_ReadBlock(unsigned char* buf, unsigned int block);
int xenon_sfc_ReadSmallBlock(unsigned char* buf, unsigned int block);
int xenon_sfc_ReadBlockSeparate(unsigned char* user, unsigned char* spare, unsigned int block);
int xenon_sfc_ReadSmallBlockSeparate(unsigned char* user, unsigned char* spare, unsigned int block);
int xenon_sfc_ReadBlockUser(unsigned char* buf, unsigned int block);
int xenon_sfc_ReadBlockSpare(unsigned char* buf, unsigned int block);
int xenon_sfc_ReadSmallBlockUser(unsigned char* buf, unsigned int block);
int xenon_sfc_ReadSmallBlockSpare(unsigned char* buf, unsigned int block);
int xenon_sfc_WriteBlock(unsigned char* buf, unsigned int block);
int xenon_sfc_ReadBlocks(unsigned char* buf, unsigned int block, unsigned int block_cnt);
int xenon_sfc_WriteBlocks(unsigned char* buf, unsigned int block, unsigned int block_cnt);
int xenon_sfc_ReadFullFlash(unsigned char* buf);
int xenon_sfc_WriteFullFlash(unsigned char* buf);
int xenon_sfc_EraseBlock(unsigned int block);
int xenon_sfc_EraseBlocks(unsigned int block, unsigned int block_cnt);

void xenon_sfc_ReadMapData(unsigned char* buf, unsigned int startaddr, unsigned int total_len);

bool xenon_sfc_GetNandStruct(xenon_nand* xe_nand);
int xenon_sfc_init_one(void);

#endif
