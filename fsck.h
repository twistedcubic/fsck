#ifndef _FSCK_H_
#define _FSCK_H_

#define BSIZE 512  // block size           
//extern void* bitStart; //start of bitmap

// File system super block                                                      
struct superblock {
  uint size;         // Size of file system image (blocks)                      
  uint nblocks;      // Number of data blocks                                   
  uint ninodes;      // Number of inodes.                                       
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure                                                      
struct dinode {
  short type;           // File type                                            
  short major;          // Major device number (T_DEV only)                     
  short minor;          // Minor device number (T_DEV only)                     
  short nlink;          // Number of links to inode in file system              
  uint size;            // Size of file (bytes)                                 
  uint addrs[NDIRECT+1];   // Data block addresses                              
};

// Inodes per block.                                                            
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i                                                     
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block                                                        
#define BPB           (BSIZE*8)

// Block containing bit for block b                 
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)
//address offset of given block from fsptr
//#define ABLOCK(blockNumber, ninodes) ((1+1+ninodes/IPB+1+1+blockNumber)*BSIZE)
#define ABLOCK(blockNumber, ninodes) blockNumber*BSIZE

// Directory is a file containing a sequence of dirent structures.    
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#endif // _FSCK_H_       
