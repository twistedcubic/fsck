#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fsck.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

int loop = 0, addrR = 0, deR = 0; //addrs[] recursion, dirent recursion
uint bitStart;
struct superblock *sb;
struct dinode *root; //root inode
struct dinode *inodeStart; //start of inode list
void *fsptr; //fs beginning
struct dinode *lf; //lost+found dinode
int nextlf = 0;
int badI; //inum with bad type
int nl[201] = {0}; //nlinks, start with nl[1] for root inode
void clear(ushort);
void setBit(ushort);
int walk(ushort, ushort);
struct dinode *getI(ushort);

int main(int argc, char **argv)
{
  //  void *fsptr; //fs beginning
  //  struct superblock *sb; //superblock
  struct stat buf;
  // int fd; //file descriptor to fs

  //open fs.img
  int fd = open(argv[1], O_RDWR);
  fstat(fd, &buf);
  int fsize = buf.st_size; //get fs size

  //fsptr = (void *)malloc(fsize);
  fsptr = mmap(NULL, fsize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  sb = (struct superblock*)((void*)fsptr + BSIZE); //super block starts at second block

  printf("size:%d nblocks:%d ninodes%d\n", sb->size, sb->nblocks, sb->ninodes);
  void *bitS = fsptr + BBLOCK(0, sb->ninodes)*BSIZE; //start of bits
  bitStart = (long)(fsptr + BBLOCK(0, sb->ninodes)*BSIZE); //start of data bitmap

  //size sanity check
  printf("file size:%d mapped size:%d\n ", sb->size, fsize/512);
  if(sb->size != fsize/BSIZE)
    {printf("Err_or: file size too big\n");
      sb->size = fsize/BSIZE;
    }
  //printf("updated size: %d\n", (int)sb->size);
  //consistency checks
  //size of inode list check
  if(sb->ninodes > sb->size || sb->nblocks > sb->size)
    {printf("Error: inode list larger than file size\n");
      exit(1);
    }
  //enumerate the free map by going through the inodes
  //inode refers to free/unused block if type is 0
  //change inode type if necessary
  //set the bitmap all to 0!!
   memset(bitS, 0, sb->nblocks/8 + 1); //8 is #of bits in byte
  printf("inum 26->addrs[0]:%p \n", fsptr+getI(26)->addrs[0]*BSIZE);
  printf("inum 27->addrs[0]:%p \n", fsptr+getI(27)->addrs[0]*BSIZE);
    
  //  struct dinode *in;
  inodeStart = (struct dinode*)((void*)sb + BSIZE); //start of inode list
  int i;
  for(i = 1; i < sb->ninodes; i++)
    {
      if(getI(i)->addrs[0] != 0)
	setBit(i); //pass in the inum!
      //      in = (struct dinode*)((char*)in + sizeof(struct dinode));
    }

  //root inode
  root = (struct dinode*)((void*)sb+BSIZE+sizeof(struct dinode)); 
  int h;
  for(h =0; h<NDIRECT+1;h++)
    printf("%d\t", root->addrs[h]);

  printf("root size:%d\n", root->size);
  printf("root dir addrs[]:%d\n", root->addrs[0]);
  printf("nlink:%d\n", root->nlink);
  //printf("IPB:%d\n", (int)sizeof(struct dinode));8 inodes/block

  
  //check for dot and dot dot for directories
  //walk through fs tree recursively
  walk(1, 1); //start with root

  //find inode for lost+found
  struct dinode *temp = getI(1);
  //printf("addrs[0]:%d\n", temp->addrs[0]);
  //printf("root dir size:%d\n", temp->size);
  // printf("nlink:%d\n", temp->nlink);
  //  printf("Error\n");
  //////////////////////
  struct dirent *tempD;
  ushort lfN = 0; //lf's inum
  int iC;
  for(iC=0; temp->addrs[iC] != 0; iC++) //could skip! 
    {
      tempD = (struct dirent*)(fsptr+temp->addrs[iC]*BSIZE);
      for( ; tempD->inum != 0 && tempD < tempD + 32; tempD++) //
	{
      if(strcmp(tempD->name, "lost+found")==0) //run through block of dirent's
	{lfN = tempD->inum;
	  lf = getI(tempD->inum); //set lf's inode
	  printf("l+f inum:%d\n", lfN);
	  break;
	}
	}
      if(lfN) //found l+f
	break;
    }
  ///////print nlink array before:
  int q;
  //walk through inode list
  for(q=0; q<sb->ninodes-1; q++) 
    {
      struct dinode *tempI = getI(q+1); //get inode address, skipping root
      if(tempI->addrs[0] != 0)
	printf("Before: inum:%d\t nlink: %d\n", q+1, tempI->nlink);
    }

  /////////
  //set root's nlink to 1
  //  root->nlink = 1;
  //valid inodes with no directory reference go to lost+found
  //already zeroed out nlinks and set invalid types to 0, fixed types
  struct dinode *tempI;
  struct dirent *tempDe;
  int j;
  //walk through inode list
  for(j=0; j<sb->ninodes-1; j++) 
    {
      tempI = getI(j+1); //get inode address, skipping root
      //  if(j<40)
      //	printf("first 40 inum:%d nlink:%d nl:%d \n", j+1, tempI->nlink, nl[j+1]);
      //check against nl[]:
      if(tempI->nlink != nl[j+1])
	{printf("Err_or: Bad link count:\n");
	  tempI->nlink = nl[j+1];
	}
      //put dangling inodes to l+f
      if(tempI->type != 0 && tempI->nlink == 0)
	{ printf("Err_or: no reference:\n");
	  printf("inum:%d type:%d \n", j+1, tempI->type);
	  tempDe = (struct dirent*)(fsptr + tempI->addrs[0]*BSIZE); //start of dirent's	  
	  //set parent dirent as lost&found
	  strcpy(tempDe->name, ".."); //set first dirent to ..
	  tempDe->inum = lfN; //inode number for lost&found
	  //could be file
	  //add to inode size
	  //	  tempI->size = (char)tempI->size + sizeof(struct dirent); //overwrote

	  //size in what denomination? in bytes 
	  struct dirent *newlf=(struct dirent*)(fsptr + BSIZE*lf->addrs[0]+(2+nextlf)*(sizeof(struct dirent))); //put block offset to addrs in lf node
	  newlf->inum = tempDe->inum;
	  strcpy(newlf->name, "justSnarred");
	  nextlf++; //index in lf->addrs[]
	  lf->size = (char)lf->size + sizeof(struct dirent); //update size!
	  nl[j+1]++;
	  tempI->nlink++;
	  //tempI->nlink++; //////////
	  //check for . dirent
	}
    }
  msync(fsptr, fsize, MS_SYNC);
  return 0;
}
//  root = (struct dinode*)((void*)sb+BSIZE+sizeof(struct dinode)); 
//inode list starts at 1, ie root has inode 1, ie skips 0th
struct dinode* getI(ushort inum){ //get (pointer to) dinode given inode number
  return (struct dinode*)((void*)sb + BSIZE + inum*sizeof(struct dinode));
}

//walk through fs tree recursively
int walk(ushort current, ushort parent) //pass in previous inode
{
 
  loop++;
  struct dinode *tempI, *parentI, *currentI;
  uint dotC, dotdotC; //counts for dot and dot dot dir entries
  void *blockAddr;
  struct dirent *tempDe;
  currentI = getI(current);
  parentI = getI(parent);

  //if root
  if(current == parent)
    {     printf("currentI->addrs[0]:%d\n", currentI->addrs[0]);
      printf("root inode number: %d \n", ((struct dirent*)((void*)fsptr+ABLOCK(currentI->addrs[0], sb->ninodes)))->inum);
    } 
  printf("**********currentI->type:%d \n", currentI->type);
  if(currentI->type != 1) //not a directory 
    return 0;//stop the recursion

  int iC;
  dotC = 0; dotdotC = 0;
  //run through data blocks in addrs[]
  for(iC=0; currentI->addrs[iC] != 0; iC++) //could skip!
    { printf("currentI->addrs[iC]: %d\n", (int)currentI->addrs[iC]);
//  blockNumber=currentI->addrs[iC];
      blockAddr = (void*)(fsptr + BSIZE*(currentI->addrs[iC]));
      //walk through directory entries in each block
     
      //  tempDe=(struct dirent*)blockAddr;
      for(tempDe = (struct dirent*)(blockAddr); tempDe->inum != 0 && (void*)tempDe < blockAddr+BSIZE; tempDe++)
      	{
	  if(tempDe->name != NULL)       
	    {if(strcmp(tempDe->name, ".") == 0)
		{dotC++;
		  printf("tempDe->inum:%d\t", tempDe->inum);
		  printf("tempDe->name:%s size:%d \n", tempDe->name, getI(tempDe->inum)->size);
		  continue;
		}
	      else if(strcmp(tempDe->name, "..") == 0)
		{dotdotC++;
		}
	      nl[tempDe->inum]++;
	      printf("tempDe->inum:%d\t", tempDe->inum); //printf("iC%d\t", iC);
	      struct dinode *temp = getI(tempDe->inum); 
	      printf("tempDe->name:%s \t nlink:%d type:%d size:%d \n", tempDe->name, temp->nlink, temp->type, temp->size);
	    }
	}  
    } 
  if(dotC == 0) 
    {//need to move overwritten dirents to l+f!
      struct dirent *self = (struct dirent*)(fsptr + BSIZE*(currentI->addrs[0]));
      self->inum = current; //itself ////
      strcpy(self->name, ".");
      if(currentI->size == 0)
	currentI->size = (char)currentI->size + sizeof(struct dirent);
      printf("Err_or: missing . dirent\n");
    }
  if(dotdotC==0) 
    {
      //add new dirent
      struct dirent *previous = (struct dirent*)(fsptr + BSIZE*(currentI->addrs[0])+sizeof(struct dirent)); //parent, added to second slot
      previous->inum = parent;
      strcpy(previous->name, ".." );
      if(currentI->size == 16 && dotC != 0)
        currentI->size = (char)currentI->size+sizeof(struct dirent);
      nl[parent]++;
      printf("Err_or: missing .. dirent\n");
    }
  //recurse through the fs tree
    int iS;
    for(iS=0; currentI->addrs[iS] != 0; iS++)
    {
      ///%%
      tempDe = (struct dirent*)(fsptr + BSIZE*(currentI->addrs[iS]));  
      struct dirent *initial = tempDe;
      while( tempDe->inum != 0)
	{  
	  //get inode with inum in tempDe
	  tempI = getI(tempDe->inum);
	  //	  if(strcmp(tempDe->name, ".")!=0) //self reference doesn't count
	  //  nl[tempDe->inum]++;

	  //set type to 0 if type not 0, 1, 2, 3
	  ushort ty = tempI->type;
	  if(ty != 0 && ty != 1 && ty!=2 &&ty!=3)
	    {printf("Err_or: bad type");
	      tempI->type = 0;
	      clear(tempDe->inum);
	      tempDe->inum = 0; //clear out dirent
	      strcpy(tempDe->name, "0");
	      tempDe = (struct dirent*)((char*)tempDe+sizeof(struct dirent));
	      if(tempDe > initial + 31)
		break; //a block holds 32 dirent's
	      continue; //move on
	    }else if(ty == 2 || ty == 3)
	    {
	      tempDe = (struct dirent*)((char*)tempDe+sizeof(struct dirent));
	      if((char*)tempDe > (char*)initial + 31*sizeof(struct dirent))
		break; //a block holds 32 dirent's	      
	      continue; //move on 
	    }
	  else if(tempI->type==1 && strcmp(tempDe->name, ".")!=0 && strcmp(tempDe->name, "..")!=0) //if a directory and not looping
	    {//current = ((struct dirent*)tempDe)->inum; //update current	      
	      printf("About to recurse. inum:%d, current inum:%d\n", tempDe->inum, current);
	      walk(tempDe->inum, current);
	    }
	  tempDe = (struct dirent*)((char*)tempDe+sizeof(struct dirent));
	  if((char*)tempDe > (char*)initial + 31*sizeof(struct dirent))
	    break; //a block holds 32 dirent's
	}
      
    }
  return 0;
}

//given an dinode , write in bit in bitmap for that block, change bit if different
void setBit(ushort inum)
{
  int i;
  uint blockNumber; //block number of content
  //get start of bitmap block   bitStart
  struct dinode *in = getI(inum);
  //  uint *temp = (void*)malloc(sizeof(uint));
  int bitBlock = BBLOCK(0, sb->ninodes); //block offset of start of bitmap block
  //  struct dirent *tempDe = fsptr + (struct dirent*)in->addrs[0]*BSIZE; //first dirent
  // struct dirent *initial = tempDe;
  for(i=0; in->addrs[i] != 0 && i < NDIRECT+1; i++)
    { //write 1 to bitmap
      blockNumber = (uint)(in->addrs[i]);
      //set #blockNumber bit in bitmap to 1
      *(char*)(fsptr+bitBlock*BSIZE+ blockNumber/8)= *(char*)(fsptr+bitBlock*BSIZE+ blockNumber/8) | 1<<(blockNumber%8);
    }
  
}
//clear out inode, zero out dirents
void clear(ushort inum)
{
  struct dinode *in = getI(inum);
  in->major = 0; in->minor = 0; in->nlink = 0;
  in->size = 0; 
  int i;
  for(i = 0; i< NDIRECT+1; i++)
    in->addrs[i] = 0;
  //zero out dirents with inum -- done in walk()
}

void findRoot(){ //find root inode
  struct dinode *root, *tempI;
  uint block; //block numbers pointed to by inode
  int rootFound = 0;
  for(tempI = inodeStart; tempI<(struct dinode*)(inodeStart+sb->ninodes); tempI++)
    {
      if(tempI->type != 1) //not a directory
	continue;
      int ib;
      void* blockAddr;
      for(ib =0; tempI->addrs[ib]!=0; ib++) //through blocks in addrs[]
	{
	  block = tempI->addrs[ib];
	  blockAddr = (void*)(fsptr + ABLOCK(block, sb->ninodes));
	  struct dirent *de;
	  //search through dir entries in each block
	  for(de = (struct dirent*)blockAddr; de->name != NULL; de++)
	    {if(*(de->name) == 'r') 
		{rootFound = 1;
		  root = tempI;
		  printf("root name:%s \n", de->name);
		  break;
		}
	    }
	  if(rootFound == 1)
	    break;
	}
      if(rootFound == 1)
	break;
    }
}
