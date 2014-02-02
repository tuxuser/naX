/*
 *  Xenon System Flash Filesystem
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

/*
 * https://www.kernel.org/doc/htmldocs/mtdnand/
 * http://www.informit.com/articles/article.aspx?p=1187102&seqNum=1
 */

#include "string.h"
#include "vsprintf.h"
#include "xenon_sfc.h"
#include "xenon_nandfs.h"


static xenon_nand nand = {0};
static DUMPDATA dumpdata;


void xenon_nandfs_CalcECC(unsigned int *data, unsigned char* edc) {
	unsigned int i=0, val=0;
	unsigned int v=0;

	for (i = 0; i < 0x1066; i++)
	{
		if (!(i & 31))
			v = ~__builtin_bswap32(*data++);
		val ^= v & 1;
		v>>=1;
		if (val & 1)
			val ^= 0x6954559;
		val >>= 1;
	}

	val = ~val;

	// 26 bit ecc data
	edc[0] = ((val << 6) | (data[0x20C] & 0x3F)) & 0xFF;
	edc[1] = (val >> 2) & 0xFF;
	edc[2] = (val >> 10) & 0xFF;
	edc[3] = (val >> 18) & 0xFF;
}

unsigned short xenon_nandfs_GetLBA(METADATA* meta)
{
	unsigned short ret = 0;
	
	switch (nand.MetaType)
	{
		case META_TYPE_SM:
			ret =  (((meta->sm.BlockID0&0xF)<<8)+(meta->sm.BlockID1));
			break;
		case META_TYPE_BOS:
			ret =  (((meta->bos.BlockID0&0xF)<<8)+(meta->bos.BlockID1&0xFF));
			break;
		case META_TYPE_BG:
			ret =  (((meta->bg.BlockID0&0xF)<<8)+(meta->bg.BlockID1&0xFF));
			break;
	}
	return ret;
}

unsigned char xenon_nandfs_GetBlockType(METADATA* meta)
{
	unsigned char ret = 0;
	
	switch (nand.MetaType)
	{
		case META_TYPE_SM:
			ret =  (meta->sm.FsBlockType&0x3F);
			break;
		case META_TYPE_BOS:
			ret =  (meta->bos.FsBlockType&0x3F);
			break;
		case META_TYPE_BG:
			ret =  (meta->bg.FsBlockType&0x3F);
			break;
	}
	return ret;
}

unsigned char xenon_nandfs_GetBadBlockMark(METADATA* meta)
{
	unsigned char ret = 0;
	
	switch (nand.MetaType)
	{
		case META_TYPE_SM:
			ret =  meta->sm.BadBlock;
			break;
		case META_TYPE_BOS:
			ret =  meta->bos.BadBlock;
			break;
		case META_TYPE_BG:
			ret =  meta->bg.BadBlock;
			break;
	}
	return ret;
}

unsigned int xenon_nandfs_GetFsSize(METADATA* meta)
{
	unsigned int ret = 0;
	
	switch (nand.MetaType)
	{
		case META_TYPE_SM:
			ret = ((meta->sm.FsSize0<<8)+meta->sm.FsSize1);
			break;
		case META_TYPE_BOS:
			ret = (((meta->bos.FsSize0<<8)&0xFF)+(meta->bos.FsSize1&0xFF));
			break;
		case META_TYPE_BG:
			ret = (((meta->bg.FsSize0&0xFF)<<8)+(meta->bg.FsSize1&0xFF));
			break;
	}
	return ret;
}

unsigned int xenon_nandfs_GetFsFreepages(METADATA* meta)
{
	unsigned int ret = 0;
	
	switch (nand.MetaType)
	{
		case META_TYPE_SM:
			ret =  meta->sm.FsPageCount;
			break;
		case META_TYPE_BOS:
			ret =  meta->bos.FsPageCount;
			break;
		case META_TYPE_BG:
			ret =  (meta->bg.FsPageCount * 4);
			break;
	}
	return ret;
}

unsigned int xenon_nandfs_GetFsSequence(METADATA* meta)
{
	unsigned int ret = 0;
	
	switch (nand.MetaType)
	{
		case META_TYPE_SM:
			ret =  (meta->sm.FsSequence0+(meta->sm.FsSequence1<<8)+(meta->sm.FsSequence2<<16));
			break;
		case META_TYPE_BOS:
			ret =  (meta->bos.FsSequence0+(meta->bos.FsSequence1<<8)+(meta->bos.FsSequence2<<16));
			break;
		case META_TYPE_BG:
			ret =  (meta->bg.FsSequence0+(meta->bg.FsSequence1<<8)+(meta->bg.FsSequence2<<16));
			break;
	}
	return ret;
}

bool xenon_nandfs_CheckMMCAnchorSha(unsigned char* buf)
{
	//unsigned char* data = buf;
	//CryptSha(&data[MMC_ANCHOR_HASH_LEN], (0x200-MMC_ANCHOR_HASH_LEN), NULL, 0, NULL, 0, sha, MMC_ANCHOR_HASH_LEN);
	return 0;
}

unsigned short xenon_nandfs_GetMMCAnchorVer(unsigned char* buf)
{
	unsigned char* data = buf;
	unsigned short tmp = (data[MMC_ANCHOR_VERSION_POS]<<8|data[MMC_ANCHOR_VERSION_POS+1]);

	return tmp;
}

unsigned short xenon_nandfs_GetMMCMobileBlock(unsigned char* buf, unsigned char mobi)
{
	unsigned char* data = buf;
	unsigned char mob = mobi - MOBILE_BASE;
	unsigned char offset = MMC_ANCHOR_MOBI_START+(mob*MMC_ANCHOR_MOBI_SIZE);
	unsigned short tmp = data[offset]<<8|data[offset+1];
	
	return tmp;
}

unsigned short xenon_nandfs_GetMMCMobileSize(unsigned char* buf, unsigned char mobi)
{
	unsigned char* data = buf;
	unsigned char mob = mobi - MOBILE_BASE;
	unsigned char offset = MMC_ANCHOR_MOBI_START+(mob*MMC_ANCHOR_MOBI_SIZE)+0x2;
	unsigned short tmp = data[offset]<<8|data[offset+1];
	
	return __builtin_bswap16(tmp);
}

bool xenon_nandfs_CheckECC(PAGEDATA* pdata)
{
	unsigned char ecd[4];
	xenon_nandfs_CalcECC((unsigned int*)pdata->User, ecd);
	if ((ecd[0] == pdata->Meta.sm.ECC0) &&
		(ecd[1] == pdata->Meta.sm.ECC1) &&
		(ecd[2] == pdata->Meta.sm.ECC2) &&
		(ecd[3] == pdata->Meta.sm.ECC3))
		return 0;
	return 1;
}

int xenon_nandfs_ReadFileByIndex(unsigned char* buf, int index)
{
	unsigned int fsFileSize, fsBlock, block_num;
	unsigned int fsStartBlock = dumpdata.FSStartBlock<<3; // Convert to Small Block
	unsigned char tmpdata[0x4000];

	fsFileSize = xenon_nandfs_GetFileSzByIndex(index);
	
	block_num = 0;
	while(fsFileSize > 0x4000)
	{
		fsBlock = fsStartBlock + dumpdata.FsEntBlocks[index][block_num];
		if(nand.MMC)
			xenon_sfc_ReadMapData(&buf[block_num*0x4000], (fsBlock*0x4000), 0x4000);
		else
			xenon_sfc_ReadSmallBlockUser(&buf[block_num*0x4000], fsBlock);
		
		fsFileSize = fsFileSize-0x4000;
		block_num++;
	}
	fsBlock = fsStartBlock + dumpdata.FsEntBlocks[index][block_num];
	if(nand.MMC)
		xenon_sfc_ReadMapData(&buf[block_num*0x4000], (fsBlock*0x4000), fsFileSize);
	else
	{
		xenon_sfc_ReadSmallBlockUser(tmpdata, fsBlock);
		memcpy(&buf[block_num*0x4000], tmpdata, fsFileSize);
	}
	
	return 0;
}

int xenon_nandfs_GetFileSzByIndex(int index)
{
	return __builtin_bswap32(dumpdata.FsEnt[index]->ClusterSz);
}

int xenon_nandfs_GetIndexByFileName(unsigned char *filename)
{
	int i;
	for(i=0;i<MAX_FSENT;i++)
	{
		if(!strncmp((const char *)filename,dumpdata.FsEnt[i]->FileName,strlen((const char *)filename)))
			return i;
	}
	return -1;
}

int xenon_nandfs_CreateFsEntryBlockmap(void)
{
	unsigned int i, k;
	unsigned int fsBlock, realBlock, block_num;
	unsigned int fsFileSize;
	unsigned int fsStartBlock = dumpdata.FSStartBlock<<3; // Convert to Small Block
//	FS_TIME_STAMP timeSt;
	
	dumpdata.pFSRootBufShort = (unsigned short*)dumpdata.FSRootBuf;
	
	for(i=0; i<MAX_FSENT; i++)
	{
		dumpdata.FsEnt[i] = (FS_ENT*)&dumpdata.FSRootFileBuf[i*sizeof(FS_ENT)];
		if(dumpdata.FsEnt[i]->FileName[0] != 0)
		{
			printf("file: %s ", dumpdata.FsEnt[i]->FileName);
			for(k=0; k< (22-strlen(dumpdata.FsEnt[i]->FileName)); k++)
			{
				printf(" ");
			}
			
			fsBlock = __builtin_bswap16(dumpdata.FsEnt[i]->StartCluster);
			fsFileSize = __builtin_bswap32(dumpdata.FsEnt[i]->ClusterSz);
			
			printf("start: %04x size: %08x stamp: %08x\n", fsBlock, fsFileSize, (unsigned int)__builtin_bswap32(dumpdata.FsEnt[i]->TypeTime));

			// extract the file
			if(dumpdata.FsEnt[i]->FileName[0] != 0x5) // file is erased but still in the record
			{
				block_num = 0;
				realBlock = fsBlock;
				if(nand.isBB)
				{
					realBlock = ((dumpdata.LBAMap[fsBlock]<<3)-fsStartBlock);
				}
				
				while(fsFileSize > 0x4000)
				{
#ifdef DEBUG
					printf("%04x:%04x, ", fsBlock, realBlock);
#endif
					dumpdata.FsEntBlocks[i][block_num] = realBlock;
					fsFileSize = fsFileSize-0x4000;
					fsBlock = __builtin_bswap16(dumpdata.pFSRootBufShort[fsBlock]); // gets next block
					realBlock = dumpdata.LBAMap[fsBlock];
					if(nand.isBB)
					{
						realBlock = (realBlock<<3); // to SmallBlock
						realBlock -= fsStartBlock; // relative Adress
						realBlock += (fsBlock % 8); // smallBlock inside bigBlock 
					}
					block_num++;
				}
				if((fsFileSize > 0)&&(fsBlock<0x1FFE))
				{
#ifdef DEBUG
					printf("%04x:%04x, ", fsBlock, realBlock);
#endif
					dumpdata.FsEntBlocks[i][block_num] = realBlock;
				}
				else
					printf("** File ended unexpected! %04x:%04x, ", fsBlock, realBlock);
			}
			else
				printf("   erased still has entry???");		
			printf("\n\n");
		}
	} 
	return 0;
}

int xenon_nandfs_ParseLBA(void)
{
	int block, spare;
	unsigned char userbuf[nand.BlockSz];
	unsigned char sparebuf[nand.MetaSz*nand.PagesInBlock];
	int FsStart = dumpdata.FSStartBlock;
	int FsSize = dumpdata.FSSize;
	unsigned short lba, lba_cnt=0;
	METADATA *meta;
		
	if(nand.MMC)
	{
		for(block=0;block<nand.BlocksCount;block++)
		{
			dumpdata.LBAMap[lba_cnt] = block; // Hail to the phison, just this one time
			lba_cnt++;
		}
	}
	else
	{
		for(block=FsStart;block<(FsStart+FsSize);block++)
		{
			xenon_sfc_ReadBlockSeparate(userbuf, sparebuf, block);
			if(nand.isBB)
			{
				for(spare=0;spare<(nand.MetaSz*nand.PagesInBlock);spare+=(nand.MetaSz*32))
				{
					meta = (METADATA*)&sparebuf[spare]; // 8 SmBlocks inside BgBlock for LBA
					lba = xenon_nandfs_GetLBA(meta);
					dumpdata.LBAMap[lba_cnt] = lba;
					lba_cnt++;
				}	
			}
			else
			{
				meta = (METADATA*)sparebuf;
				lba = xenon_nandfs_GetLBA(meta);
				dumpdata.LBAMap[lba_cnt] = lba;
				lba_cnt++;
			}
		}
	}
	printf("Read 0x%x LBA entries\n",lba_cnt);
	return 0;
}

int xenon_nandfs_SplitFsRootBuf()
{
	int block = dumpdata.FSRootBlock;
	unsigned int i, j, root_off, file_off, ttl_off;
	unsigned char data[nand.BlockSz];
	
	if(nand.MMC)
		xenon_sfc_ReadMapData(data, (block*nand.BlockSz), nand.BlockSz);
	else
		xenon_sfc_ReadBlockUser(data, block);
	
	root_off = 0;
	file_off = 0;
	ttl_off = 0;
	for(i=0; i<16; i++) // copy alternating 512 bytes into each buf
	{
		for(j=0; j<nand.PageSz; j++)
		{
			dumpdata.FSRootBuf[root_off+j] = data[ttl_off+j];
			dumpdata.FSRootFileBuf[file_off+j] = data[ttl_off+j+512];
		}
		root_off += nand.PageSz;
		file_off += nand.PageSz;
		ttl_off  += (nand.PageSz*2);
	}

#ifdef FSROOT_WRITE_OUT
	writeToFile("fsrootbuf.bin", dumpdata.FSRootBuf, FSROOT_SIZE);
	writeToFile("fsrootfilebuf.bin", dumpdata.FSRootFileBuf, FSROOT_SIZE);
#endif
	return 0;
}

bool xenon_nandfs_init(void)
{
	unsigned char mobi, fsroot_ident;
	unsigned int i, j, mmc_anchor_blk, prev_mobi_ver, prev_fsroot_ver, tmp_ver, blk, size = 0, page_each;
	bool ret = false;
	unsigned char anchor_num = 0;
	char mobileName[] = {"MobileA"};
	METADATA* meta;

	if(nand.MMC)
	{
		unsigned char blockbuf[nand.BlockSz * 2];
		mmc_anchor_blk = nand.ConfigBlock - MMC_ANCHOR_BLOCKS;
		prev_mobi_ver = 0;
		
		xenon_sfc_ReadMapData(blockbuf, (mmc_anchor_blk * nand.BlockSz), (nand.BlockSz * 2));
		
		for(i=0; i < MMC_ANCHOR_BLOCKS; i++)
		{
			tmp_ver = xenon_nandfs_GetMMCAnchorVer(&blockbuf[i*nand.BlockSz]);
			if(tmp_ver >= prev_mobi_ver) 
			{
				prev_mobi_ver = tmp_ver;
				anchor_num = i;
			}
		}
		
		if(prev_mobi_ver == 0)
		{
			printf("MMC Anchor block wasn't found!");
			return false;
		}


		for(mobi = 0x30; mobi < 0x3F; mobi++)
		{
			blk = xenon_nandfs_GetMMCMobileBlock(&blockbuf[anchor_num*nand.BlockSz], mobi);
			size = xenon_nandfs_GetMMCMobileSize(&blockbuf[anchor_num*nand.BlockSz], mobi);

			if(blk == 0)
				continue;

			if(mobi == MOBILE_FSROOT)
			{
				printf("FSRoot found at block 0x%x (off: 0x%x), v %i, size %d (0x%x) bytes\n", blk, (blk*nand.BlockSz), prev_mobi_ver, nand.BlockSz, nand.BlockSz);
				dumpdata.FSRootBlock = blk;
				dumpdata.FSRootVer = prev_mobi_ver; // anchor version
				ret  = true;
			}
			else
			{
				mobileName[6] = mobi+0x11;
				printf("%s found at block 0x%x (off: 0x%x), v %i, size %d (0x%x) bytes\n", mobileName, blk, (blk*nand.BlockSz), prev_mobi_ver, size*nand.BlockSz, size*nand.BlockSz);
				dumpdata.Mobile[mobi-MOBILE_BASE].Version = prev_mobi_ver;
				dumpdata.Mobile[mobi-MOBILE_BASE].Block = blk;
				dumpdata.Mobile[mobi-MOBILE_BASE].Size = size * nand.BlockSz;
			}
		}
	}
	else
	{
		unsigned char userbuf[nand.BlockSz];
		unsigned char sparebuf[nand.MetaSz*nand.PagesInBlock];
		
		if(nand.isBB) // Set FSroot Identifier, depending on nandtype
			fsroot_ident = BB_MOBILE_FSROOT;
		else
			fsroot_ident = MOBILE_FSROOT;
		
		for(blk=0; blk < nand.BlocksCount; blk++)
		{
			xenon_sfc_ReadBlockSeparate(userbuf, sparebuf, blk);
			meta = (METADATA*)sparebuf;
			
			mobi = xenon_nandfs_GetBlockType(meta);
			tmp_ver = xenon_nandfs_GetFsSequence(meta);

			if(mobi == fsroot_ident) // fs root
			{
				prev_fsroot_ver = dumpdata.FSRootVer; // get current version
				if(tmp_ver >= prev_fsroot_ver) 
				{
					dumpdata.FSRootVer = tmp_ver; // assign new version number
					dumpdata.FSRootBlock = blk;
				}	
				else
					continue;
					
				printf("FSRoot found at block 0x%x (off: 0x%x), v %i, size %d (0x%x) bytes\n", blk, (blk*nand.BlockSzPhys), dumpdata.FSRootVer, nand.BlockSz, nand.BlockSz);
				
				if(nand.isBB)
				{
					dumpdata.MUStart = meta->bg.FsSize1;
					dumpdata.FSSize = (meta->bg.FsSize0<<2);
					dumpdata.FSStartBlock = nand.SizeUsableFs - meta->bg.FsPageCount - dumpdata.FSSize;
				}
				else
				{
					dumpdata.MUStart = 0;
					dumpdata.FSSize = nand.BlocksCount;
					dumpdata.FSStartBlock = 0;
				}
				
				ret = true;
			}
			else if((mobi >= MOBILE_BASE) && (mobi < MOBILE_END)) //Mobile*.dat
			{	
				prev_mobi_ver = dumpdata.Mobile[mobi-MOBILE_BASE].Version;
				if(tmp_ver >= prev_mobi_ver)
					dumpdata.Mobile[mobi-MOBILE_BASE].Version = tmp_ver;
				else
					continue;

				page_each = nand.PagesInBlock - xenon_nandfs_GetFsFreepages(meta);
				//printf("pageEach: %x\n", pageEach);
				// find the most recent instance in the block and dump it
				j = 0;
				for(i=0; i < nand.PagesInBlock; i += page_each)
				{
					meta = (METADATA*)&sparebuf[nand.MetaSz*i];
					//printf("i: %d type: %x\n", i, meta->FsBlockType);
					if(xenon_nandfs_GetBlockType(meta) == (mobi))
						j = i;
					if(xenon_nandfs_GetBlockType(meta) == 0x3F)
						i = nand.PagesInBlock;
				}
			
				meta = (METADATA*)&sparebuf[j*nand.MetaSz];
				size = xenon_nandfs_GetFsSize(meta);
				
				dumpdata.Mobile[mobi-MOBILE_BASE].Size = size;
				dumpdata.Mobile[mobi-MOBILE_BASE].Block = blk;
				
				mobileName[6] = mobi+0x31;
				printf("%s found at block 0x%x (off: 0x%x), page %d, v %i, size %d (0x%x) bytes\n", mobileName, blk, (blk*nand.BlockSzPhys), j, tmp_ver, size, size);
			}
		}
	}
	return ret;
}

bool xenon_nandfs_init_one(void)
{
	int ret;
	
	ret = xenon_sfc_GetNandStruct(&nand);
	if(!ret)
	{
		printf("Failed to get enumerated NAND information\n");
		goto err_out;
	}
	
	ret = xenon_nandfs_init();
	if(!ret)
	{
		printf("FSRoot wasn't found\n");
		goto err_out;
	}
	
	xenon_nandfs_ParseLBA();
	xenon_nandfs_SplitFsRootBuf();
	xenon_nandfs_CreateFsEntryBlockmap();
	return true;
	
	err_out:
		memset (&nand, 0, sizeof(DUMPDATA));
		memset (&dumpdata, 0, sizeof(xenon_nand));
		return false;
}
