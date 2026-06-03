#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <string.h>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <sstream>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

// helper functions

bool bitAvailable(int bit_pos, unsigned char* bitmap) {
  unsigned char img_byte = bitmap[bit_pos / 8]; // target position
  unsigned char mask =  1 << (bit_pos % 8); // make a bit mask
  if ((mask & img_byte) == 0) { // bitwise AND, if 0 its available, else its in use
    return true;
  }
  else  {
    return false; //bit_pos in use  
  }
}

int byteToBlocks(int size) {
  int blocks = size / UFS_BLOCK_SIZE; // find # of blocks directory has
  if ((size % UFS_BLOCK_SIZE) != 0) { // if not multiple of 4096, add a block
    blocks += 1;
  }
  return blocks;
}

void getDirTable(inode_t dirInode, Disk* disk, int dirTableSize, dir_ent_t* dirTable) {
  int blocks = byteToBlocks(dirInode.size); // determine number of blocks in data region to read

  // read data blocks that contains dir table information
  unsigned char* dirTableBuffer = new unsigned char[blocks * UFS_BLOCK_SIZE]; // buffer to hold blocks
  for (int i = 0; i < blocks; i++) {
    int dataBlockNum = dirInode.direct[i]; // address
    unsigned char dataBlock[UFS_BLOCK_SIZE]; // hold one block's data
    disk->readBlock(dataBlockNum, dataBlock); // read data block and put into temp
    memcpy(dirTableBuffer + (i * UFS_BLOCK_SIZE), dataBlock, UFS_BLOCK_SIZE); // copy block data into buffer
  }

  // transfer dirTableBuffer into dirTable
  for (int i = 0; i < dirTableSize; i++) {
    dir_ent_t tempDirEntry;
    memcpy(&tempDirEntry, dirTableBuffer + (i * sizeof(dir_ent_t)), sizeof(dir_ent_t)); // copy data into temp dirEntry
    dirTable[i] = tempDirEntry; // assign to corresponding index
  }
  
  delete[] dirTableBuffer;
}

void getFileContent(inode_t* fileInode, Disk* disk, int size, unsigned char* fileContent) {
  int blocks = byteToBlocks(size); // determine # of blocks in data region to read

  // read data blocks with file content
  unsigned char* fileContentBuffer = new unsigned char[blocks * UFS_BLOCK_SIZE]; // buffer to hold file content
  for (int i = 0; i < blocks; i++) {
    int dataBlockNumber = fileInode->direct[i]; // get data block index
    unsigned char dataBlock[UFS_BLOCK_SIZE]; 
    disk->readBlock(dataBlockNumber, dataBlock); // read from disk into temp data block
    memcpy(fileContentBuffer + (i * UFS_BLOCK_SIZE), dataBlock, UFS_BLOCK_SIZE); // copy into buffer
  }

  for (int i = 0; i < blocks; i++) {
    int readSize = 0;
    if (i == (blocks - 1)) { // check if i is on the last block
      readSize = size % UFS_BLOCK_SIZE; // calculate leftover bytes
      if (readSize == 0) { // if no leftover
        readSize = UFS_BLOCK_SIZE; // read whole block
      }
    } 
    else {
      readSize = UFS_BLOCK_SIZE; // read full block if not on last block
    }  
    memcpy(fileContent + (i * UFS_BLOCK_SIZE), fileContentBuffer + (i * UFS_BLOCK_SIZE), readSize); // copy from buffer into fileContent
  }

  delete[] fileContentBuffer;
}

// member functions*********************************************************************************

LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super) { 
  unsigned char readBuffer[UFS_BLOCK_SIZE]; // hold 1 disk block
  disk->readBlock(0, readBuffer); // super block at 0 on disk
  memcpy(super, readBuffer, sizeof(super_t)); // copy bytes of readbuffer to super
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) { 
  unsigned char tempBuffer[UFS_BLOCK_SIZE]; // hold 1 disk block

  for (int i = 0; i < super->inode_bitmap_len; i++) { // loop through every disk block for inode bitmap
    disk->readBlock(super->inode_bitmap_addr + i, tempBuffer); // read current block, starting at base and adding i block offset
    memcpy(inodeBitmap + (i * UFS_BLOCK_SIZE), tempBuffer, UFS_BLOCK_SIZE); // copy block into inodebitmap, offset by i*4096 bytes
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {

}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) { 
  unsigned char tempBuffer[UFS_BLOCK_SIZE]; // hold 1 disk block

  for (int i = 0; i < super->data_bitmap_len; i++) { // loop through every disk block for data bitmap
    disk->readBlock(super->data_bitmap_addr + i, tempBuffer); // read current block, starting at base and adding i block offset
    memcpy(dataBitmap + (i * UFS_BLOCK_SIZE), tempBuffer, UFS_BLOCK_SIZE); // copy block into inodebitmap, offset by i*4096 bytes
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {

}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  unsigned char tempBuffer[UFS_BLOCK_SIZE]; // hold 1 disk block
  unsigned char *inodeBuffer = new unsigned char[super->inode_region_len * UFS_BLOCK_SIZE]; // buffer to hold inode region

  // read 4KB inode blocks from disk
  for (int i = 0; i < super->inode_region_len; i++) {
    disk->readBlock(super->inode_region_addr + i, tempBuffer); // read block from disk and put into tempbuffer
    memcpy(inodeBuffer + (i * UFS_BLOCK_SIZE), tempBuffer, UFS_BLOCK_SIZE); // copy block to place in buffer by offset
  }

  // transfer inode blocks into array of inodes
  for (int i = 0; i < super->num_inodes; i++) {
    inode_t tempInode;
    memcpy(&tempInode, inodeBuffer + (i * sizeof(inode_t)), sizeof(inode_t)); // copy offset inode block into tempinode
    inodes[i] = tempInode; // assign to corresponding inode and inode number
  }

  delete[] inodeBuffer;
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  unsigned char tempBuffer[UFS_BLOCK_SIZE]; // hold 1 disk block
  unsigned char *inodeBuffer = new unsigned char[super->inode_region_len * UFS_BLOCK_SIZE]; // buffer to hold inode region

  // transfer array of inodes to inode blocks
  for (int i = 0; i < super->num_inodes; i++) {
    inode_t tempInode;
    tempInode = inodes[i];
    memcpy(inodeBuffer + (i * sizeof(inode_t)), &tempInode, sizeof(inode_t)); // copy inode to its place in buffer by offset
  }

  // write 4KB inode blocks to disk one at a time
  for (int i = 0; i < super->inode_region_len; i++) {
    memcpy(tempBuffer, inodeBuffer + (i * UFS_BLOCK_SIZE), UFS_BLOCK_SIZE); // copy from buffer to tempbuffer
    disk->writeBlock(super->inode_region_addr + i, tempBuffer); // write block to correct address
  }

  delete[] inodeBuffer;
}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  super_t super;
  readSuperBlock(&super);

  inode_t parentInode;
  if (stat(parentInodeNumber, &parentInode) < 0) { // get parent data
    return -EINVALIDINODE;
  }

  if ((parentInodeNumber < 0) || (parentInodeNumber >= super.num_inodes) || (parentInode.type != UFS_DIRECTORY)) { // check if parent inode is valid
    return -EINVALIDINODE; 
  } 
  else {
    // populate dir table
    int dirTableSize = parentInode.size/sizeof(dir_ent_t); // get number of slots
    dir_ent_t dirTable[dirTableSize];
    getDirTable(parentInode, disk, dirTableSize, dirTable);

    //use name to search for inode number in dirTable
    for (int i = 0; i < dirTableSize; i++) { // iterate through dirTable to find string match
      if (name == dirTable[i].name) {
        return dirTable[i].inum; // match found, return inode number entry
      }
    }
    return -EINVALIDINODE; // not found
  }
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  super_t super;
  readSuperBlock(&super);

  if (inodeNumber < 0 || inodeNumber >= super.num_inodes) { // check if inodenumber within bounds
    return -EINVALIDINODE;
  }

  // return error if inode not in use 
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE]; // allocate buffer
  readInodeBitmap(&super, inodeBitmap); 
  if (bitAvailable(inodeNumber, inodeBitmap)) { // check if inode in use
    return -EINVALIDINODE;
  }

  // get inode table
  inode_t inodeTable[super.num_inodes];
  readInodeRegion(&super, inodeTable);

  *inode = inodeTable[inodeNumber]; // copy inode
  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  super_t super;
  inode_t inode;
  readSuperBlock(&super);
  stat(inodeNumber, &inode);

  // check for valid inode number
  if ((inodeNumber < 0) || (inodeNumber >= super.num_inodes)) {
    return -EINVALIDINODE;
  }
  // check for size
  if (size < inode.size) { // if size larger than size of object, return bytes of object
    size = inode.size;
  }

  if (inode.type == UFS_DIRECTORY) {
    int dirTableSize = inode.size/sizeof(dir_ent_t); // get number of directory entries
    dir_ent_t dirTable[dirTableSize];
    getDirTable(inode, disk, dirTableSize, dirTable); // get directory entries from disk
    memcpy(buffer, dirTable, size); // copy directory table to buffer
  } 
  else { // UFS_REGULAR_FILE
    unsigned char fileContent[size];
    getFileContent(&inode, disk, size, fileContent); // get file data from disk
    memcpy(buffer, fileContent, size); // copy file content to buffer
  }

  return size; // return bytes read
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  return 0;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  return 0;
}

