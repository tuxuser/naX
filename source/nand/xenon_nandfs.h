#ifndef _XENON_NANDFS_H
#define _XENON_NANDFS_H

#define MMC_ANCHOR_BLOCKS 		2
#define MMC_ANCHOR_HASH_LEN		0x14
#define MMC_ANCHOR_VERSION_POS	0x1A
#define MMC_ANCHOR_MOBI_START	0x1C
#define MMC_ANCHOR_MOBI_SIZE	0x4

#define MAX_MOBILE				0xF
#define MOBILE_BASE				0x30
#define MOBILE_END				0x3F

#define MOBILE_FSROOT			0x30
#define BB_MOBILE_FSROOT		0x2C

#define FSROOT_SIZE				0x2000

#define MOBILE_PB			32				// pages counting towards FsPageCount
#define MOBILE_MULTI		1				// small block multiplier for (MOBILE_PB-FsPageCount)
#define BB_MOBILE_PB		(MOBILE_PB*2)	// pages counting towards FsPageCount
#define BB_MOBILE_MULTI		4				// small block multiplier for (BB_MOBILE_PB-FsPageCount)

#define MAX_LBA				0x1000
#define MAX_FSENT			256

typedef struct _METADATA_SMALLBLOCK{
	unsigned char BlockID1; // lba/id = (((BlockID0&0xF)<<8)+(BlockID1))
	unsigned char BlockID0 : 4;
	unsigned char FsUnused0 : 4;
	//unsigned char BlockID0; 
	unsigned char FsSequence0; // oddly these aren't reversed
	unsigned char FsSequence1;
	unsigned char FsSequence2;
	unsigned char BadBlock;
	unsigned char FsSequence3;
	unsigned char FsSize1; // ((FsSize0<<8)+FsSize1) = cert size
	unsigned char FsSize0;
	unsigned char FsPageCount; // free pages left in block (ie: if 3 pages are used by cert then this would be 29:0x1d)
	unsigned char FsUnused1[0x2];
	unsigned char FsBlockType : 6;
	unsigned char ECC3 : 2;
	unsigned char ECC2; // 14 bit ECD
	unsigned char ECC1;
	unsigned char ECC0;
} SMALLBLOCK, *PSMALLBLOCK;

typedef struct _METADATA_BIGONSMALL{
	unsigned char FsSequence0;
	unsigned char BlockID1; // lba/id = (((BlockID0<<8)&0xF)+(BlockID1&0xFF))
	unsigned char BlockID0 : 4; 
	unsigned char FsUnused0 : 4;
	unsigned char FsSequence1;
	unsigned char FsSequence2;
	unsigned char BadBlock;
	unsigned char FsSequence3;
	unsigned char FsSize1; // (((FsSize0<<8)&0xFF)+(FsSize1&0xFF)) = cert size
	unsigned char FsSize0;
	unsigned char FsPageCount; // free pages left in block (ie: if 3 pages are used by cert then this would be 29:0x1d)
	unsigned char FsUnused1[2];
	unsigned char FsBlockType : 6;
	unsigned char ECC3 : 2;
	unsigned char ECC2; // 26 bit ECD
	unsigned char ECC1;
	unsigned char ECC0;
} BIGONSMALL, *PBIGONSMALL;

typedef struct _METADATA_BIGBLOCK{
	unsigned char BadBlock;
	unsigned char BlockID1; // lba/id = (((BlockID0&0xF)<<8)+(BlockID1&0xFF))
	unsigned char BlockID0 : 4;
	unsigned char FsUnused0 : 4;
	unsigned char FsSequence2; // oddly, compared to before these are reversed...?
	unsigned char FsSequence1;
	unsigned char FsSequence0;
	unsigned char FsUnused1;
	unsigned char FsSize1; //FS: 06 (system reserve block number) else ((FsSize0<<16)+(FsSize1<<8)) = cert size
	unsigned char FsSize0; // FS: 20 (size of flash filesys in smallblocks >>5)
	unsigned char FsPageCount; // FS: 04 (system config reserve) free pages left in block (multiples of 4 pages, ie if 3f then 3f*4 pages are free after)
	unsigned char FsUnused2[0x2];
	unsigned char FsBlockType : 6; // FS: 2a bitmap: 2c (both use FS: vals for size), mobiles
	unsigned char ECC3 : 2;
	unsigned char ECC2; // 26 bit ECD
	unsigned char ECC1;
	unsigned char ECC0;
} BIGBLOCK, *PBIGBLOCK;

typedef struct _METADATA{
	union{
		SMALLBLOCK sm;
		BIGONSMALL bos;
		BIGBLOCK bg;
	};
} METADATA, *PMETADATA;

typedef struct _PAGEDATA{
	unsigned char User[512];
	METADATA Meta;
} PAGEDATA, *PPAGEDATA;

typedef struct _FS_TIME_STAMP{
	unsigned int DoubleSeconds : 5;
	unsigned int Minute : 6;
	unsigned int Hour : 5;
	unsigned int Day : 5;
	unsigned int Month : 4;
	unsigned int Year : 7;
} FS_TIME_STAMP;

typedef struct _FS_ENT{
	char FileName[22];
	unsigned short StartCluster; //unsigned char startCluster[2];
	unsigned int ClusterSz; //unsigned char clusterSz[4];
	unsigned int TypeTime;
} FS_ENT, *PFS_ENT;

typedef struct _MOBILE_ENT{
	unsigned char Version;
	unsigned short Block;
	unsigned int Size;
} MOBILE_ENT, *PMOBILE_ENT;

typedef struct _DUMPDATA{
	unsigned short MUStart;
	unsigned short FSSize;
	unsigned short FSStartBlock;
	unsigned short FSRootBlock;
	unsigned short FSRootVer;
	unsigned short LBAMap[MAX_LBA];
	unsigned char FSRootBuf[FSROOT_SIZE];
	unsigned short* pFSRootBufShort;
	unsigned char FSRootFileBuf[FSROOT_SIZE];
	MOBILE_ENT Mobile[MAX_MOBILE];
	FS_ENT *FsEnt[MAX_FSENT];
	unsigned short FsEntBlocks[MAX_FSENT][0x100];
} DUMPDATA, *PDUMPDATA;

void xenon_nandfs_CalcECC(unsigned int* data, unsigned char* edc);
unsigned short xenon_nandfs_GetLBA(METADATA* meta);
unsigned char xenon_nandfs_GetBlockType(METADATA* meta);
unsigned char xenon_nandfs_GetBadblockMark(METADATA* meta);
unsigned int xenon_nandfs_GetFsSize(METADATA* meta);
unsigned int xenon_nandfs_GetFsFreepages(METADATA* meta);
unsigned int xenon_nandfs_GetFsSequence(METADATA* meta);
bool xenon_nandfs_CheckMMCAnchorSha(unsigned char* buf);
unsigned short xenon_nandfs_GetMMCAnchorVer(unsigned char* buf);
unsigned short xenon_nandfs_GetMMCMobileBlock(unsigned char* buf, unsigned char mobi);
unsigned short xenon_nandfs_GetMMCMobileSize(unsigned char* buf, unsigned char mobi);
bool xenon_nandfs_CheckECC(PAGEDATA* pdata);

int xenon_nandfs_ReadFileByIndex(unsigned char *buf, int index);
int xenon_nandfs_GetFileSzByIndex(int index);
int xenon_nandfs_GetIndexByFileName(unsigned char *filename);

int xenon_nandfs_ExtractFsEntry(void);
int xenon_nandfs_ParseLBA(void);
int xenon_nandfs_SplitFsRootBuf(void);
bool xenon_nandfs_init(void);
bool xenon_nandfs_init_one(void);

#endif
