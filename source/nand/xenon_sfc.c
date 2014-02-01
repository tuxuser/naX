/*
 *  Xenon System Flash Controller for Xbox 360
 *
 *  Copyright (C) 2014 tuxuser
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <malloc.h>
#include "string.h"
#include "vsprintf.h"
#include "xenon_sfc.h"

#define DRV_NAME	"xenon_sfc"
#define DRV_VERSION	"0.1"

#define SFC_ADDR 0x80000200ea00c000ULL
#define SFC_SIZE 0x400

#define DMA_SIZE 0x10000

#define MAP_ADDR 0x80000200C8000000ULL
#define MAP_SIZE 0x4000000

#define DEBUG_OUT 1

typedef struct _xenon_sfc
{
	unsigned long long dmaaddr;
	unsigned char *dmabuf;
	xenon_nand nand;
} xenon_sfc, *pxenon_sfc;

static xenon_sfc sfc;

inline unsigned long xenon_sfc_ReadReg(unsigned int addr)
{
	return __builtin_bswap32(*(volatile unsigned int*)(SFC_ADDR | addr));
}

inline void xenon_sfc_WriteReg(unsigned int addr, unsigned long data)
{
	*(volatile unsigned int*)(SFC_ADDR | addr) = __builtin_bswap32(data);
}

int xenon_sfc_EraseBlock(unsigned int block)
{
	int status;
	int addr = block * sfc.nand.BlockSz;
	
	// Enable Writes
	xenon_sfc_WriteReg(SFCX_CONFIG, xenon_sfc_ReadReg(SFCX_CONFIG) | CONFIG_WP_EN);
	xenon_sfc_WriteReg(SFCX_STATUS, 0xFF);

	// Set flash address (logical)
	//address &= 0x3fffe00; // Align to page
	xenon_sfc_WriteReg(SFCX_ADDRESS, addr);

	// Wait Busy
	while (xenon_sfc_ReadReg(SFCX_STATUS) & STATUS_BUSY);

	// Unlock sequence (for erase)
	xenon_sfc_WriteReg(SFCX_COMMAND, UNLOCK_CMD_1);
	xenon_sfc_WriteReg(SFCX_COMMAND, UNLOCK_CMD_0);

	// Wait Busy
	while (xenon_sfc_ReadReg(SFCX_STATUS) & STATUS_BUSY);

	// Command the block erase
	xenon_sfc_WriteReg(SFCX_COMMAND, BLOCK_ERASE);

	// Wait Busy
	while (xenon_sfc_ReadReg(SFCX_STATUS) & STATUS_BUSY);

	status = xenon_sfc_ReadReg(SFCX_STATUS);
	//if (!SFCX_SUCCESS(status))
	//	printf(" ! SFCX: Unexpected sfc.erase_block status %08X\n", status);
	xenon_sfc_WriteReg(SFCX_STATUS, 0xFF);

	// Disable Writes
	xenon_sfc_WriteReg(SFCX_CONFIG, xenon_sfc_ReadReg(SFCX_CONFIG) & ~CONFIG_WP_EN);

	return status;
}

int _xenon_sfc_ReadPage(unsigned char* buf, unsigned int page, bool raw)
{
	int status, i, PageSz;
	int addr = page * sfc.nand.PageSz;
	unsigned char* data = buf;

	xenon_sfc_WriteReg(SFCX_STATUS, xenon_sfc_ReadReg(SFCX_STATUS));

	// Set flash address (logical)
	//address &= 0x3fffe00; // Align to page
	xenon_sfc_WriteReg(SFCX_ADDRESS, addr);

	// Command the read
	// Either a logical read (0x200 bytes, no meta data)
	// or a Physical read (0x210 bytes with meta data)
	xenon_sfc_WriteReg(SFCX_COMMAND, raw ? PHY_PAGE_TO_BUF : LOG_PAGE_TO_BUF);

	// Wait Busy
	while ((status = xenon_sfc_ReadReg(SFCX_STATUS)) & STATUS_BUSY);

	if (!SFCX_SUCCESS(status))
	{
		if (status & STATUS_BB_ER)
			printf(" ! SFCX: Bad block found at %08X\n", addr/sfc.nand.BlockSz);
		else if (status & STATUS_ECC_ER)
		//	printf(" ! SFCX: (Corrected) ECC error at address %08X: %08X\n", address, status);
			status = status;
		else if (!raw && (status & STATUS_ILL_LOG))
			printf(" ! SFCX: Illegal logical block at %08X (status: %08X)\n", addr/sfc.nand.BlockSz, status);
		else
			printf(" ! SFCX: Unknown error at address %08X: %08X. Please worry.\n", addr, status);
	}

	// Set internal page buffer pointer to 0
	xenon_sfc_WriteReg(SFCX_ADDRESS, 0);

	PageSz = raw ? sfc.nand.PageSzPhys : sfc.nand.PageSz;

	for (i = 0; i < PageSz ; i += 4)
	{
		// Transfer data from buffer to register
		xenon_sfc_WriteReg(SFCX_COMMAND, PAGE_BUF_TO_REG);

		// Read out our data through the register
		*(int*)(data + i) = __builtin_bswap32(xenon_sfc_ReadReg(SFCX_DATA));
	}

	return status;
}

int xenon_sfc_ReadPagePhy(unsigned char* buf, unsigned int page)
{
	return _xenon_sfc_ReadPage(buf, page, 1);
}

int xenon_sfc_ReadPageLog(unsigned char* buf, unsigned int page)
{
	return _xenon_sfc_ReadPage(buf, page, 0);
}

int xenon_sfc_WritePage(unsigned char* buf, unsigned int page)
{
	int i, status;
	int addr = page * sfc.nand.PageSz;
	unsigned char* data = buf;
	
	xenon_sfc_WriteReg(SFCX_STATUS, 0xFF);

	// Enable Writes
	xenon_sfc_WriteReg(SFCX_CONFIG, xenon_sfc_ReadReg(SFCX_CONFIG) | CONFIG_WP_EN);

	// Set internal page buffer pointer to 0
	xenon_sfc_WriteReg(SFCX_ADDRESS, 0);

	for (i = 0; i < sfc.nand.PageSzPhys; i+=4)
	{
		// Write out our data through the register
		xenon_sfc_WriteReg(SFCX_DATA, __builtin_bswap32(*(int*)(data + i)));

		// Transfer data from register to buffer
		xenon_sfc_WriteReg(SFCX_COMMAND, REG_TO_PAGE_BUF);
	}

	// Set flash address (logical)
	//address &= 0x3fffe00; // Align to page
	xenon_sfc_WriteReg(SFCX_ADDRESS, addr);

	// Unlock sequence (for write)
	xenon_sfc_WriteReg(SFCX_COMMAND, UNLOCK_CMD_0);
	xenon_sfc_WriteReg(SFCX_COMMAND, UNLOCK_CMD_1);

	// Wait Busy
	while (xenon_sfc_ReadReg(SFCX_STATUS) & STATUS_BUSY);

	// Command the write
	xenon_sfc_WriteReg(SFCX_COMMAND, WRITE_PAGE_TO_PHY);

	// Wait Busy
	while (xenon_sfc_ReadReg(SFCX_STATUS) & STATUS_BUSY);

	status = xenon_sfc_ReadReg(SFCX_STATUS);
	if (!SFCX_SUCCESS(status))
		printf(" ! SFCX: Unexpected sfc.writepage status %08X\n", status);

	// Disable Writes
	xenon_sfc_WriteReg(SFCX_CONFIG, xenon_sfc_ReadReg(SFCX_CONFIG) & ~CONFIG_WP_EN);

	return status;
}

// returns 1 on bad block, 2 on unrecoverable ECC
// data NULL to skip keeping data
int xenon_sfc_ReadBlock(unsigned char* buf, unsigned int block)
{
	int i, status, block_read;
	int addr = block * sfc.nand.BlockSz;
	
	unsigned char* data = buf;
	unsigned int cur_addr = addr;
	
	if(buf != NULL)
		memset(data, 0, (sfc.nand.PagesInBlock*sfc.nand.PageSzPhys));

	for(block_read = 0; block_read < sfc.nand.BlockSzPhys; block_read += 0x2000, cur_addr += 0x2000)
	{
		xenon_sfc_WriteReg(SFCX_STATUS, xenon_sfc_ReadReg(SFCX_STATUS));
		xenon_sfc_WriteReg(SFCX_DATAPHYADDR, sfc.dmaaddr);
		xenon_sfc_WriteReg(SFCX_SPAREPHYADDR, sfc.dmaaddr+0xC000);
		xenon_sfc_WriteReg(SFCX_ADDRESS, cur_addr);
		xenon_sfc_WriteReg(SFCX_COMMAND, DMA_PHY_TO_RAM);
		
		while(xenon_sfc_ReadReg(SFCX_STATUS) & STATUS_BUSY);
		
		status = xenon_sfc_ReadReg(SFCX_STATUS);
		if(status&STATUS_ERROR)
		{
			printf("error in status, %08x\n", status);
			if(status&STATUS_BB_ER)
			{
				printf("Bad block error block 0x%x\n", block);
				return 1;
			}
			if(STSCHK_ECC_ERR(status))
			{
				printf("unrecoverable ECC error block 0x%x\n", block);
				return 2;
			}
		}
		if(buf != NULL)
		{
			for(i = 0; i < 16; i++)
			{
				memcpy(data, &sfc.dmabuf[i*sfc.nand.PageSz], sfc.nand.PageSz);
				memcpy(&data[sfc.nand.PageSz], &sfc.dmabuf[0xC000+(i*sfc.nand.MetaSz)], sfc.nand.MetaSz);
				data += sfc.nand.PageSzPhys;
			}
		}
	}
	return 0;
}

int xenon_sfc_ReadSmallBlock(unsigned char* buf, unsigned int block)
{
	int i, status, block_read;
	// hardcoding these for big block nand-type
	unsigned int BlockSz = 0x4000;
	unsigned int BlockSzPhys = 0x4200;
	unsigned short PageSz = 0x200;
	unsigned short PageSzPhys = 0x210;
	unsigned char MetaSz = 0x10;
	unsigned char PagesInBlock = 32;
	int addr = block * BlockSz;
	unsigned char* data = buf;
	unsigned int cur_addr = addr;
	
	if(buf != NULL)
		memset(data, 0, (PagesInBlock*PageSzPhys));

	for(block_read = 0; block_read < BlockSzPhys; block_read += 0x2000, cur_addr += 0x2000)
	{
		xenon_sfc_WriteReg(SFCX_STATUS, xenon_sfc_ReadReg(SFCX_STATUS));
		xenon_sfc_WriteReg(SFCX_DATAPHYADDR, sfc.dmaaddr);
		xenon_sfc_WriteReg(SFCX_SPAREPHYADDR, sfc.dmaaddr+0xC000);
		xenon_sfc_WriteReg(SFCX_ADDRESS, cur_addr);
		xenon_sfc_WriteReg(SFCX_COMMAND, DMA_PHY_TO_RAM);
		
		while(xenon_sfc_ReadReg(SFCX_STATUS) & STATUS_BUSY);
		
		status = xenon_sfc_ReadReg(SFCX_STATUS);
		if(status&STATUS_ERROR)
		{
			printf("error in status, %08x\n", status);
			if(status&STATUS_BB_ER)
			{
				printf("Bad block error block 0x%x\n", block);
				return 1;
			}
			if(STSCHK_ECC_ERR(status))
			{
				printf("unrecoverable ECC error block 0x%x\n", block);
				return 2;
			}
		}
		if(buf != NULL)
		{
			for(i = 0; i < 16; i++)
			{
				memcpy(data, &sfc.dmabuf[i*PageSz], PageSz);
				memcpy(&data[PageSz], &sfc.dmabuf[0xC000+(i*MetaSz)], MetaSz);
				data += PageSzPhys;
			}
		}
	}
	return 0;
}

int xenon_sfc_ReadBlockSeparate(unsigned char* user, unsigned char* spare, unsigned int block)
{
	int config, wconfig, i;
	unsigned char* data = (unsigned char *)malloc(sfc.nand.BlockSzPhys);
	
	if(user && spare)
	{
		config = xenon_sfc_ReadReg(SFCX_CONFIG);
		wconfig = (config&~(CONFIG_DMA_LEN|CONFIG_INT_EN|CONFIG_WP_EN));
		if(sfc.nand.isBB)
			wconfig = wconfig|CONFIG_DMA_PAGES(4); // change to 4 pages, bb 4 pages = 16 small pages
		else
			wconfig = wconfig|CONFIG_DMA_PAGES(16); // change to 16 pages
		xenon_sfc_WriteReg(SFCX_CONFIG, wconfig);

// 		printf("Reading block %x of %x at block %x (%x)\n", blk, block_cnt, blk+block, (blk+block)*sfc.nand.BlockSzPhys);
		xenon_sfc_ReadSmallBlock(data, block);

		xenon_sfc_WriteReg(SFCX_CONFIG, config);
	}
	else
		printf("supplied buffers weren't allocated for readblock_separate\n");
	
	for(i = 0; i < sfc.nand.PagesInBlock; i++)
	{
		memcpy(user, &data[i*sfc.nand.PageSzPhys], sfc.nand.PageSz);
		memcpy(spare, &data[(i*sfc.nand.PageSzPhys)+sfc.nand.PageSz], sfc.nand.MetaSz); 
		user += sfc.nand.PageSz;
		spare += sfc.nand.MetaSz;
	}
	free(data);
	
	return 0;	
}

int xenon_sfc_ReadSmallBlockSeparate(unsigned char* user, unsigned char* spare, unsigned int block)
{
	int config, wconfig, i;
	unsigned int BlockSzPhys = 0x4200;
	unsigned short PageSz = 0x200;
	unsigned short PageSzPhys = 0x210;
	unsigned char MetaSz = 0x10;
	unsigned char PagesInBlock = 32;
	unsigned char* data = (unsigned char *)malloc(BlockSzPhys);
	
	if(user && spare)
	{
		config = xenon_sfc_ReadReg(SFCX_CONFIG);
		wconfig = (config&~(CONFIG_DMA_LEN|CONFIG_INT_EN|CONFIG_WP_EN));
		if(sfc.nand.isBB)
			wconfig = wconfig|CONFIG_DMA_PAGES(4); // change to 4 pages, bb 4 pages = 16 small pages
		else
			wconfig = wconfig|CONFIG_DMA_PAGES(16); // change to 16 pages
		xenon_sfc_WriteReg(SFCX_CONFIG, wconfig);

// 		printf("Reading block %x of %x at block %x (%x)\n", blk, block_cnt, blk+block, (blk+block)*sfc.nand.BlockSzPhys);
		xenon_sfc_ReadBlock(data, block);

		xenon_sfc_WriteReg(SFCX_CONFIG, config);
	}
	else
		printf("supplied buffers weren't allocated for readblock_separate\n");
	
	for(i = 0; i < PagesInBlock; i++)
	{
		memcpy(user, &data[i*PageSzPhys], PageSz);
		memcpy(spare, &data[(i*PageSzPhys)+PageSz], MetaSz); 
		user += PageSz;
		spare += MetaSz;
	}
	free(data);
	
	return 0;	
}

int xenon_sfc_ReadBlockUser(unsigned char* buf, unsigned int block)
{
	unsigned char* tmp = (unsigned char *)malloc(sfc.nand.MetaSz*sfc.nand.PagesInBlock);
	xenon_sfc_ReadBlockSeparate(buf, tmp, block);
	free(tmp);
	return 0;
}

int xenon_sfc_ReadBlockSpare(unsigned char* buf, unsigned int block)
{
	unsigned char* tmp = (unsigned char *)malloc(sfc.nand.BlockSz);
	xenon_sfc_ReadBlockSeparate(tmp, buf, block);
	free(tmp);
	return 0;
}

int xenon_sfc_ReadSmallBlockUser(unsigned char* buf, unsigned int block)
{
	unsigned char PagesInBlock = 32;
	unsigned char MetaSz = 0x10;
	unsigned char* tmp = (unsigned char *)malloc(MetaSz*PagesInBlock);
	xenon_sfc_ReadSmallBlockSeparate(buf, tmp, block);
	free(tmp);
	return 0;
}

int xenon_sfc_ReadSmallBlockSpare(unsigned char* buf, unsigned int block)
{
	unsigned short BlockSz = 0x4000;
	unsigned char* tmp = (unsigned char *)malloc(BlockSz);
	xenon_sfc_ReadSmallBlockSeparate(tmp, buf, block);
	free(tmp);
	return 0;
}

int xenon_sfc_WriteBlock(unsigned char* buf, unsigned int block)
{
	int status, i, block_wrote;
	int addr = block * sfc.nand.BlockSz;
	
	unsigned char* data = buf;
	unsigned int cur_addr = addr;
	
	// one erase per block
	status = xenon_sfc_EraseBlock(addr);
	if(status&STATUS_ERROR)
		printf("error in erase status, %08x\n", status);

	if(buf != NULL)
	{
		for(block_wrote = 0; block_wrote < sfc.nand.BlockSzPhys; block_wrote += 0x2000, cur_addr += 0x2000)
		{
			for(i = 0; i < 16; i++)
			{
				memcpy(&sfc.dmabuf[i*sfc.nand.PageSz], data, sfc.nand.PageSz);
				memcpy(&sfc.dmabuf[0xC000+(i*sfc.nand.MetaSz)], &data[sfc.nand.PageSz], sfc.nand.MetaSz);
				data += sfc.nand.PageSzPhys;
			}
			xenon_sfc_WriteReg(SFCX_STATUS, xenon_sfc_ReadReg(SFCX_STATUS));
			xenon_sfc_WriteReg(SFCX_COMMAND, UNLOCK_CMD_0);
			xenon_sfc_WriteReg(SFCX_COMMAND, UNLOCK_CMD_1);
			xenon_sfc_WriteReg(SFCX_DATAPHYADDR, sfc.dmaaddr);
			xenon_sfc_WriteReg(SFCX_SPAREPHYADDR, sfc.dmaaddr+0xC000);
			xenon_sfc_WriteReg(SFCX_ADDRESS, cur_addr);
			xenon_sfc_WriteReg(SFCX_COMMAND, DMA_RAM_TO_PHY);
			
			while(xenon_sfc_ReadReg(SFCX_STATUS) & STATUS_BUSY);
			
			status = xenon_sfc_ReadReg(SFCX_STATUS);
			if(status&STATUS_ERROR)
				printf("error in status, %08x\n", status);
		}
	}
	return 0;
}

int xenon_sfc_ReadBlocks(unsigned char* buf, unsigned int block, unsigned int block_cnt)
{
	int cur_blk, config, wconfig;
	//int sz = (block_cnt*sfc.nand.BlockSzPhys);
	//unsigned char *buf = (unsigned char *)malloc(block_cnt*sfc.nand.BlockSzPhys);
	//unsigned char *buf = (unsigned char *)VirtualAlloc(0, (block_cnt*sfc.nand.BlockSzPhys), MEM_COMMIT|MEM_LARGE_PAGES, PAGE_READWRITE);
	
	if(buf)
	{
// 		printf("reading %d blocks starting at %x block, %x sz\n", block_cnt, block, sz);
		if(sfc.nand.MMC)
		{
/*
			int rd = ReadFlash(block*sfc.nand.BlockSzPhys, buf, sz, sfc.nand.BlockSzPhys, NULL);
			if(rd != sz)
			{
				printf("trying to read MMC yielded 0x%x bytes instead of 0x%x!\n", rd, sz);
				VirtualFree(buf,0, MEM_RELEASE );
				return NULL;
			}
*/
			printf("MMC not yet implemented!\n");
		}
		else
		{
			config = xenon_sfc_ReadReg(SFCX_CONFIG);
			wconfig = (config&~(CONFIG_DMA_LEN|CONFIG_INT_EN|CONFIG_WP_EN));
			if(sfc.nand.isBB)
				wconfig = wconfig|CONFIG_DMA_PAGES(4); // change to 4 pages, bb 4 pages = 16 small pages
			else
				wconfig = wconfig|CONFIG_DMA_PAGES(16); // change to 16 pages
			xenon_sfc_WriteReg(SFCX_CONFIG, wconfig);
			
			for(cur_blk = 0; cur_blk < block_cnt; cur_blk++)
			{
// 				printf("Reading block %x of %x at block %x (%x)\n", blk, block_cnt, blk+block, (blk+block)*sfc.nand.BlockSzPhys);
				xenon_sfc_ReadBlock(&buf[cur_blk*sfc.nand.BlockSzPhys], cur_blk+block);
			}
			xenon_sfc_WriteReg(SFCX_CONFIG, config);
		}
// 		printf("flash read complete\n");
	}
	else
		printf("supplied buffer wasn't allocated for readblocks\n");
		
	return 0;
}

int xenon_sfc_BlockHasData(unsigned char* buf)
{
	int i;
	unsigned char *data = buf;
	
	for(i = 0; i < sfc.nand.BlockSzPhys; i++)
		if((data[i]&0xFF) != 0xFF)
			return 1;
	return 0;
}

int xenon_sfc_WriteBlocks(unsigned char *buf, unsigned int block, unsigned int block_cnt)
{
	int cur_blk, config, wconfig;
	
	unsigned char* blk_data;
	unsigned char* data = buf;
	//int sz = (block_cnt*sfc.nand.BlockSzPhys);
	unsigned char* blockbuf = (unsigned char *)malloc(sfc.nand.BlockSzPhys);
	
	if(((block+block_cnt)*sfc.nand.BlockSzPhys) > sfc.nand.SizeDump)
	{
		printf("error, write exceeds system area!\n");
		return 0;
	}
	
	if(sfc.nand.MMC)
	{
/*
		int wr;
		wr = WriteFlash(block*sfc.nand.BlockSzPhys, data, sz, sfc.nand.BlockSzPhys, NULL);
		if(wr != sz)
		{
			printf("trying to write MMC yielded 0x%x bytes instead of 0x%x!\n", wr, sz);
			return 0;
		}
*/
		printf("MMC not yet implemented!\n");
	}
	else
	{
		config = xenon_sfc_ReadReg(SFCX_CONFIG);
		wconfig = (config &~(CONFIG_DMA_LEN|CONFIG_INT_EN))|CONFIG_WP_EN; // for the write
		if(sfc.nand.isBB)
			wconfig = wconfig|CONFIG_DMA_PAGES(4); // change to 4 pages, bb 4 pages = 16 small pages
		else
			wconfig = wconfig|CONFIG_DMA_PAGES(16); // change to 16 pages
		xenon_sfc_WriteReg(SFCX_CONFIG, (wconfig));
		
		for(cur_blk = 0; cur_blk < block_cnt; cur_blk++)
		{
			blk_data = &data[cur_blk*sfc.nand.BlockSzPhys];
			// check for bad block, do NOT modify bad blocks!!!
			if(xenon_sfc_ReadBlock(blockbuf, cur_blk+block) == 0)
			{
				// check if data needs to be written
				if(memcmp(blockbuf, blk_data, sfc.nand.BlockSzPhys) != 0)
				{
					// check if block has data to write, or if it's only an erase
					if(xenon_sfc_BlockHasData(blk_data))
					{
						//printf("Writing block %x of %x at %x (%x)\n", cur_blk, block_cnt, cur_blk+block, (cur_blk+block)*sfc.nand.BlockSzPhys);
						xenon_sfc_WriteBlock(blk_data, cur_blk+block);
					}
					else
					{
						//printf("Erase only block %x of %x at %x (%x)\n", cur_blk, block_cnt, cur_blk+block, (cur_blk+block)*sfc.nand.BlockSzPhys);
						xenon_sfc_WriteBlock(NULL, cur_blk+block);
					}
				}
				//else
				//	printf("skipping write block %x at %x of %x, data identical\n", cur_blk, cur_blk*sfc.nand.BlockSzPhys, sfc.nand.SizeData/sfc.nand.BlockSzPhys);
			}
			else
				printf("not writing to block %x, it is bad!\n", cur_blk+block);
		}
		xenon_sfc_WriteReg(SFCX_CONFIG, config);
	}
// 	printf("flash write complete\n");
	return 0;
}

int xenon_sfc_EraseBlocks(unsigned int block, unsigned int block_cnt)
{
	int cur_blk, config, wconfig, status;
	
	//int sz = (block_cnt*sfc.nand.BlockSzPhys);
	if(sfc.nand.MMC)
	{
/*
		int i;
		memset(g_blockBuf, 0xFF, sfc.nand.BlockSzPhys);
		for(i = 0; i < block_cnt; i++)
		{
			int wr = WriteFlash((block+i)*sfc.nand.BlockSzPhys, g_blockBuf, sfc.nand.BlockSzPhys, sfc.nand.BlockSzPhys, NULL);
			if(wr != sz)
			{
				printf("trying to fake-erase MMC yielded 0x%x bytes instead of 0x%x!\n", wr, sz);
				return 0;
			}
		}
*/
		printf("MMC not yet implemented!\n");
	}
	else
	{
		config = xenon_sfc_ReadReg(SFCX_CONFIG);
		wconfig = (config &~(CONFIG_DMA_LEN|CONFIG_INT_EN))|CONFIG_WP_EN; // for the write
		if(sfc.nand.isBB)
			wconfig = wconfig|CONFIG_DMA_PAGES(4); // change to 4 pages, bb 4 pages = 16 small pages
		else
			wconfig = wconfig|CONFIG_DMA_PAGES(16); // change to 16 pages
		xenon_sfc_WriteReg(SFCX_CONFIG, (wconfig));
		
		for(cur_blk = 0; cur_blk < block_cnt; cur_blk++)
		{
			status = xenon_sfc_EraseBlock(cur_blk+block);
			if(status&STATUS_ERROR)
				printf("error in erase status, %08x\n", status);
		}
		xenon_sfc_WriteReg(SFCX_CONFIG, config);
	}
	return 0;
}

void xenon_sfc_ReadMapData(unsigned char* buf, unsigned int startaddr, unsigned int total_len)
{
	memcpy(buf, (const void *)(MAP_ADDR|startaddr), total_len);
} 

int xenon_sfc_WriteFullFlash(unsigned char* buf)
{
	int cur_blk, config, wconfig;
	unsigned char* data;
	unsigned char* blockbuf = (unsigned char *)malloc(sfc.nand.BlockSzPhys);
// 	printf("writing flash\n");

	if(sfc.nand.MMC)
	{
/*		int wr;
		wr = WriteFlash(0, nandData, sfc.nand.SizeDump, sfc.nand.BlockSzPhys, &workerSize);
		if(wr != sfc.nand.SizeDump)
		{
			printf("trying to write MMC yielded 0x%x bytes instead of 0x%x!\n", wr, sfc.nand.SizeDump);
			return 0;
		}
*/
		printf("MMC not yet implemented!\n");
	}
	else
	{
		config = xenon_sfc_ReadReg(SFCX_CONFIG);
		wconfig = (config&~(CONFIG_DMA_LEN|CONFIG_INT_EN))|CONFIG_WP_EN; // for the write
		if(sfc.nand.isBB)
			wconfig = wconfig|CONFIG_DMA_PAGES(4); // change to 4 pages, bb 4 pages = 16 small pages
		else
			wconfig = wconfig|CONFIG_DMA_PAGES(16); // change to 16 pages
		xenon_sfc_WriteReg(SFCX_CONFIG, (wconfig));
		for(cur_blk = 0; cur_blk < (sfc.nand.SizeData/sfc.nand.BlockSzPhys); cur_blk++)
		{
			data = &buf[cur_blk*sfc.nand.BlockSzPhys];

			// check for bad block, do NOT modify bad blocks!!!
			if(xenon_sfc_ReadBlock(blockbuf, cur_blk) == 0)
			{
				// check if data needs to be written
				if(memcmp(blockbuf, data, sfc.nand.BlockSzPhys) != 0)
				{
					// check if block has data to write, or if it's only an erase
					if(xenon_sfc_BlockHasData(data))
					{
 						//printf("Writing block %x at %x of %x\n", cur_blk, cur_blk*sfc.nand.BlockSzPhys, sfc.nand.SizeData/sfc.nand.BlockSzPhys);
						xenon_sfc_WriteBlock(data, cur_blk);
					}
					else
					{
						if(sfc.nand.isBB)
						{
							//PPAGEDATA ppd = (PPAGEDATA)blockbuf;	/* TODO: Check if block needs to be written at all */
							//if((ppd[0].meta.bg.FsBlockType >= 0x2a)||(ppd[0].meta.bg.FsBlockType == 0))
								xenon_sfc_WriteBlock(NULL, cur_blk);
// 							else
// 								printf("block 0x%x contains nandmu data! type: %x\n", cur_blk, ppd[0].meta.bg.FsBlockType);
							//if((ppd[0].meta.bg.FsBlockType < 0x2a)&&(ppd[0].meta.bg.FsBlockType != 0))
							//	printf("block 0x%x contains nandmu data! type: %x\n", cur_blk, ppd[0].meta.bg.FsBlockType);
							//else
							//	writeBlock(NULL, cur_blk);
						}
						else
						{
							//printf("Erase only block %x at %x of %x\n", cur_blk, cur_blk*sfc.nand.BlockSzPhys, sfc.nand.SizeData/sfc.nand.BlockSzPhys);
							xenon_sfc_WriteBlock(NULL, cur_blk);
						}
					}
				}
				//else
				//	printf("skipping write block %x at %x of %x, data identical\n", cur_blk, cur_blk*sfc.nand.BlockSzPhys, sfc.nand.SizeData/sfc.nand.BlockSzPhys);
			}
			else
				printf("not writing to block %x, it is bad!\n", cur_blk);
		}
		xenon_sfc_WriteReg(SFCX_CONFIG, config);
	}
	free(blockbuf);
// 	printf("flash write complete\n");
	return 0;
}

int xenon_sfc_ReadFullFlash(unsigned char* buf)
{
	int cur_blk, config, wconfig;
	unsigned char* data = buf;
	
	if(sfc.nand.MMC)
	{
/*
		int rd = ReadFlash(0, nandData, sfc.nand.SizeDump, 0x20000, &workerSize);
		if(rd != sfc.nand.SizeDump)
		{
			printf("trying to read MMC yielded 0x%x bytes instead of 0x%x!\n", rd, sfc.nand.SizeDump);
			return 0;
		}
*/
	}
	else
	{
 		config = xenon_sfc_ReadReg(SFCX_CONFIG);
   		wconfig = (config&~(CONFIG_DMA_LEN|CONFIG_INT_EN|CONFIG_WP_EN));
   		if(sfc.nand.isBB)
   			wconfig = wconfig|CONFIG_DMA_PAGES(4); // change to 4 pages, bb 4 pages = 16 small pages
   		else
   			wconfig = wconfig|CONFIG_DMA_PAGES(16); // change to 16 pages
 		xenon_sfc_WriteReg(SFCX_CONFIG, wconfig);
 		
 		for(cur_blk = 0; cur_blk < (sfc.nand.SizeData/sfc.nand.BlockSzPhys); cur_blk++)
 		{
  			//printf("Reading block %x at %x of %x\n", cur_blk, cur_blk*sfc.nand.BlockSzPhys, sfc.nand.SizeData/sfc.nand.BlockSzPhys);
 			xenon_sfc_ReadBlock(&data[cur_blk*sfc.nand.BlockSzPhys], cur_blk);
 		}
 		xenon_sfc_WriteReg(SFCX_CONFIG, config);
	}
// 	printf("flash read complete\n");
	return 0;
}

bool xenon_sfc_GetNandStruct(xenon_nand* xe_nand)
{
	xe_nand = &sfc.nand;
	return xe_nand->init;
}


static bool _xenon_sfc_enum_nand(void)
{
	int config;
	u64 eMMC;
	
	sfc.nand.PagesInBlock = 32;
	sfc.nand.MetaSz = 0x10;
	sfc.nand.PageSz = 0x200;
	sfc.nand.PageSzPhys = sfc.nand.PageSz + sfc.nand.MetaSz;
	
	eMMC = xenon_sfc_ReadReg(SFCX_MMC_IDENT);
	if (eMMC != 0)
		sfc.nand.MMC = true;
	else
		sfc.nand.MMC = false;

	if(sfc.nand.MMC) // corona MMC
	{
		sfc.nand.init = true;
		sfc.nand.MetaType = META_TYPE_NONE;
		sfc.nand.BlockSz = 0x4000;
		sfc.nand.BlockSzPhys = 0x20000;
		sfc.nand.MetaSz = 0;
		
		sfc.nand.SizeDump = 0x3000000;
		sfc.nand.SizeData = 0x3000000;
		sfc.nand.SizeSpare = 0;
		sfc.nand.SizeUsableFs = 0xC00; // (sfc.nand.SizeDump/sfc.nand.BlockSz)

#ifdef DEBUG_OUT
		printf("MMC console detected\n");
#endif
	}
	else
	{
		config = xenon_sfc_ReadReg(SFCX_CONFIG);

		// defaults for 16/64M small block
		sfc.nand.MetaType = META_TYPE_SM;
		sfc.nand.BlockSz = 0x4000;
		sfc.nand.BlockSzPhys = 0x4200;
		sfc.nand.isBB = false;
		sfc.nand.isBBCont = false;

#ifdef DEBUG_OUT
		printf("SFC config %08x ver: %d type: %d\n", config, ((config>>17)&3), ((config >> 4) & 0x3));
#endif

		switch((config>>17)&3) // 00043000
		{
			case 0: // small block flash controller
				switch ((config >> 4) & 0x3)
				{
// 					 case 0: // 8M, not really supported
// 						sfc.nand.SizeDump = 0x840000;
// 						sfc.nand.SizeData = 0x800000;
// 						sfc.nand.SizeSpare = 0x40000;
// 						break;
					case 1: // 16MB
						sfc.nand.init = true;
						sfc.nand.SizeDump = 0x1080000;
						sfc.nand.SizeData = 0x1000000;
						sfc.nand.SizeSpare = 0x80000;
						sfc.nand.SizeUsableFs = 0x3E0;
						break;
// 					 case 2: // 32M, not really supported
// 						sfc.nand.SizeDump = 0x2100000;
// 						sfc.nand.SizeData = 0x2000000;
// 						sfc.nand.SizeSpare = 0x100000;
						sfc.nand.SizeUsableFs = 0x7C0;
// 						break;
					case 3: // 64MB
						sfc.nand.init = true;
						sfc.nand.SizeDump = 0x4200000;
						sfc.nand.SizeData = 0x4000000;
						sfc.nand.SizeSpare = 0x200000;
						sfc.nand.SizeUsableFs = 0xF80;
						break;
					default:
						printf("unknown T%i NAND size! (%x)\n", ((config >> 4) & 0x3), (config >> 4) & 0x3);
				}
				break;
			case 1: // big block flash controller
				switch ((config >> 4) & 0x3)// TODO: FIND OUT FOR 64M!!! IF THERE IS ONE!!!
				{
					case 1: // Small block 16MB setup
						sfc.nand.init = true;
						sfc.nand.MetaType = META_TYPE_BOS;
						sfc.nand.isBBCont = true;
						sfc.nand.SizeDump = 0x1080000;
						sfc.nand.SizeData = 0x1000000;
						sfc.nand.SizeSpare = 0x80000;
						sfc.nand.SizeUsableFs = 0x3E0;
						break;
					case 2: // Large Block: Current Jasper 256MB and 512MB
						sfc.nand.init = true;
						sfc.nand.MetaType = META_TYPE_BG;
						sfc.nand.isBBCont = true;
						sfc.nand.isBB = true;
						sfc.nand.SizeDump = 0x4200000;
						sfc.nand.BlockSzPhys = 0x21000;
						sfc.nand.SizeData = 0x4000000;
						sfc.nand.SizeSpare = 0x200000;
						sfc.nand.PagesInBlock = 256;
						sfc.nand.BlockSz = 0x20000;
						sfc.nand.SizeUsableFs = 0x1E0;
						break;
					default:
						printf("unknown T%i NAND size! (%x)\n", ((config >> 4) & 0x3), (config >> 4) & 0x3);
				}
				break;
			case 2: // MMC capable big block flash controller ie: 16M corona 000431c4
				switch ((config >> 4) & 0x3) 
				{
					case 0: // 16M
						sfc.nand.init = 1;
						sfc.nand.MetaType = META_TYPE_BOS;
						sfc.nand.isBBCont = 1;
						sfc.nand.SizeDump = 0x1080000;
						sfc.nand.SizeData = 0x1000000;
						sfc.nand.SizeSpare = 0x80000;
						sfc.nand.SizeUsableFs = 0x3E0;
						break;
					case 1: // 64M
						sfc.nand.init = 1;
						sfc.nand.MetaType = META_TYPE_BOS;
						sfc.nand.isBBCont = 1;
						sfc.nand.SizeDump = 0x4200000;
						sfc.nand.SizeData = 0x4000000;
						sfc.nand.SizeSpare = 0x200000;
						sfc.nand.SizeUsableFs = 0xF80;
						break;
					case 2: // Big Block
						sfc.nand.init = 1;
						sfc.nand.MetaType = META_TYPE_BG;
						sfc.nand.isBBCont = 1;
						sfc.nand.isBB = 1;
						sfc.nand.SizeDump = 0x4200000;
						sfc.nand.BlockSzPhys = 0x21000;
						sfc.nand.SizeData = 0x4000000;
						sfc.nand.SizeSpare = 0x200000;
						sfc.nand.PagesInBlock = 256;
						sfc.nand.BlockSz = 0x20000;
						sfc.nand.SizeUsableFs = 0x1E0;
						break;
					//case 3: // big block, but with blocks twice the size of known big blocks above...
					//	break;
					default:
						printf("unknown T%i NAND size! (%x)\n", ((config >> 4) & 0x3), (config >> 4) & 0x3);
				}
				break;
			default:
				printf("unknown NAND type! (%x)\n", (config>>17)&3);
		}
	}

	sfc.nand.ConfigBlock = sfc.nand.SizeUsableFs - CONFIG_BLOCKS;
	sfc.nand.BlocksCount = sfc.nand.SizeDump / sfc.nand.BlockSzPhys;
	sfc.nand.PagesCount = sfc.nand.BlocksCount * sfc.nand.PagesInBlock;

#ifdef DEBUG_OUT
	printf("isBBCont : %s\n", sfc.nand.isBBCont == 1 ? "TRUE":"FALSE");
	printf("isBB     : %s\n", sfc.nand.isBB == 1 ? "TRUE":"FALSE");
	printf("SizeDump       : 0x%x\n", sfc.nand.SizeDump);
	printf("BlockSz      : 0x%x\n", sfc.nand.BlockSz);
	printf("SizeData         : 0x%x\n", sfc.nand.SizeData);
	printf("SizeSpare        : 0x%x\n", sfc.nand.SizeSpare);
	printf("PagesInBlock  : 0x%x\n", sfc.nand.PagesInBlock);
	printf("SizeWrite    : 0x%x\n", sfc.nand.SizeWrite);
#endif
	return sfc.nand.init;
}

int xenon_sfc_init_one(void)
{
	// static int printed_version;
	int rc;
	
	rc = _xenon_sfc_enum_nand();
	if(!rc)
		goto err_exit;

	return 1;

err_exit:
	return 0;
}
