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



// member functions

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

}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {

}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  return 0;
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
  return 0;
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

