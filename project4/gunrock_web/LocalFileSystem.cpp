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

bool bitAvailable(int bitPos, unsigned char* bitmap) {
  unsigned char imgByte = bitmap[bitPos / 8]; // target position
  unsigned char mask =  1 << (bitPos % 8); // make a bit mask
  if ((mask & imgByte) == 0) { // bitwise AND, if 0 its available, else its in use
    return true;
  }
  else  {
    return false; //bitPos in use  
  }
}

void setBitAvailable(int bitPos, unsigned char* bitmap) {
  unsigned char imgByte = bitmap[bitPos / 8]; // target position
  unsigned char mask =  ~(1 << (bitPos % 8)); // make a bit mask with 0
  bitmap[bitPos / 8]  = mask & imgByte; // bitwise AND to make it 0
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

// finds first available 0 bit in bitmap and marks as in use, returning the index
int getAndSetAvailableBit(int totalBits, unsigned char* bitmap) {
  for (int i = 0; i < totalBits ; i++){
    int byteIdx = i / 8;
    int bitPosition = i % 8;
    
    if ((bitmap[byteIdx] & (1 << bitPosition)) == 0) { // use bitwise AND to check if the position is free (0)
      bitmap[byteIdx] |= (1 << bitPosition); // flip bit to 1 (in use)
      return i; // return index
    }
  }
  return -1; // no free bit is found 
}

// determine # of data blocks to write, update dataBitMap
int writeDataBlock(LocalFileSystem *fileSystem, int inodeNumber, const void *buffer, int size) {
  super_t super;
  fileSystem->readSuperBlock(&super);
  inode_t inode;
  int stat = fileSystem->stat(inodeNumber, &inode);
  if (stat < 0 ){
    return stat;
  } 

  int blocksToWrite = byteToBlocks(size); // # of data blocks to write
  int originalBlocks = byteToBlocks(inode.size); // original # of data blocks

  // list of data block numbers to write to
  vector<unsigned int> dataBlockNumVec; 
  for(int i = 0; i < min(originalBlocks, blocksToWrite); i++){
    dataBlockNumVec.push_back(inode.direct[i]);
  }

  unsigned char* dataBitmap = new unsigned char[super.data_bitmap_len * UFS_BLOCK_SIZE];
  fileSystem->readDataBitmap(&super, dataBitmap);
  // New blocks are needed if originalBlocks < blocks 
  // need to update dataBitmap to indicate that newly created block will be in use
  if (originalBlocks < blocksToWrite) { // file is expanding, needs new blocks
    for (int i = 0; i < (blocksToWrite - originalBlocks); i++) {
      int freeInodeIdx = getAndSetAvailableBit(super.num_data, dataBitmap);
      if (freeInodeIdx < 0) {
        delete[] dataBitmap;
        return -1; 
      } 
      
      // write to data bitmap only when there are free data blocks
      int unusedDataBlockNum = super.data_region_addr + freeInodeIdx;
      fileSystem->writeDataBitmap(&super, dataBitmap); // save bitmap changes
      dataBlockNumVec.push_back(unusedDataBlockNum);
    }
  }

  // if new entry uses fewer data blocks, free the extra data blocks
  if (blocksToWrite < originalBlocks) { // file is getting smaller
    for (int i = blocksToWrite; i < originalBlocks; i++) {
      setBitAvailable(inode.direct[i] - super.data_region_addr, dataBitmap); // mark as available in bitmap
    }
    fileSystem->writeDataBitmap(&super, dataBitmap);
  } 

  delete[] dataBitmap;

  // iterate through the list of data block numbers to transfer buffer into data blocks
  unsigned char* tempBuffer = new unsigned char[UFS_BLOCK_SIZE];
  for (size_t i = 0; i < dataBlockNumVec.size(); i++) {
    memset(tempBuffer, 0, UFS_BLOCK_SIZE); // initialize block to 0

    if (i < dataBlockNumVec.size() - 1) { // copy ful block size
      memcpy(tempBuffer, static_cast<const u_int8_t*>(buffer) + (i * UFS_BLOCK_SIZE), UFS_BLOCK_SIZE);
    }
    else { // last set of bytes to write
      int sizeLast = size % UFS_BLOCK_SIZE;
      if (sizeLast == 0) sizeLast = UFS_BLOCK_SIZE;
      memcpy(tempBuffer, static_cast<const u_int8_t*>(buffer)  + (i * UFS_BLOCK_SIZE), sizeLast);
    }
    fileSystem->disk->writeBlock(dataBlockNumVec[i], tempBuffer); // write to disk
    inode.direct[i] = dataBlockNumVec[i]; // update inode poiter
  }
  delete[] tempBuffer;

  // update inode information, and write into inodeTable
  inode.size = size; // update size property
  inode_t* inodeTable = new inode_t[super.num_inodes];
  fileSystem->readInodeRegion(&super, inodeTable); // read inode region
  inodeTable[inodeNumber] = inode; // put in updated inode
  fileSystem->writeInodeRegion(&super, inodeTable); // write it back

  delete[] inodeTable;
  return 0;
}

// member functions********************************************************************************************

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
  unsigned char tempBuffer[UFS_BLOCK_SIZE]; // hold 1 disk block

  for (int i = 0; i < super->inode_bitmap_len; i++) { // loop through every disk block for inode bitmap
    memcpy(tempBuffer, inodeBitmap + (i * UFS_BLOCK_SIZE), UFS_BLOCK_SIZE); // copy into temp buffer
    disk->writeBlock(super->inode_bitmap_addr + i, tempBuffer); // write into place in disk
  }
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) { 
  unsigned char tempBuffer[UFS_BLOCK_SIZE]; // hold 1 disk block

  for (int i = 0; i < super->data_bitmap_len; i++) { // loop through every disk block for data bitmap
    disk->readBlock(super->data_bitmap_addr + i, tempBuffer); // read current block, starting at base and adding i block offset
    memcpy(dataBitmap + (i * UFS_BLOCK_SIZE), tempBuffer, UFS_BLOCK_SIZE); // copy block into inodebitmap, offset by i*4096 bytes
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  unsigned char tempBuffer[UFS_BLOCK_SIZE]; // hold 1 disk block

  for (int i = 0; i < super->data_bitmap_len; i++) { // loop through every disk block for data bitmap
    memcpy(tempBuffer, dataBitmap + (i * UFS_BLOCK_SIZE), UFS_BLOCK_SIZE); // copy into temp buffer
    disk->writeBlock(super->data_bitmap_addr + i, tempBuffer); // write into place in disk
  }
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
    return -ENOTFOUND; // not found
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
  super_t super;
  readSuperBlock(&super);

  inode_t parentInode;

  // check for errors
  if (stat(parentInodeNumber, &parentInode) < 0) {
    return -EINVALIDINODE;
  }
  if ((type != UFS_DIRECTORY) && (type != UFS_REGULAR_FILE)) {
    return -EINVALIDTYPE;
  }
  if (name.find('/') != string::npos) {
  return -EINVALIDNAME;
  }
  if ((name.size() >= DIR_ENT_NAME_SIZE) || (name.size() < 1)) {
    return -EINVALIDNAME;
  }
  if ((name == ".") || (name == "..")) {
    return -EINVALIDNAME;
  }
  if ((parentInodeNumber < 0) || (parentInodeNumber >= super.num_inodes) || (parentInode.type != UFS_DIRECTORY)) {
    return -EINVALIDINODE;
  }

  // check if name exists
  int inodeNumber = lookup(parentInodeNumber, name);
  if (inodeNumber >= 0) { // name exists
    inode_t inode;
    if (stat(inodeNumber, &inode) < 0) {
      return -EINVALIDINODE;
    }
    if (inode.type == type) { // name exists and is right type
      return inodeNumber; 
    }
    return -EINVALIDTYPE; // exists but wrong type
  }

  // inodeNumber < 0
  // need to create new inode and write to data blocks

  // update inodeBitmap since newly created inode
  unsigned char* inodeBitmap = new unsigned char[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inodeBitmap);
  int newInodeNumber = getAndSetAvailableBit(super.num_inodes, inodeBitmap); // find free slot
  if (newInodeNumber < 0) { // no available inodes
    delete[] inodeBitmap;
    return -ENOTENOUGHSPACE;
  }
  writeInodeBitmap(&super, inodeBitmap); // update bitmap

  // add new inode to inode table
  inode_t* inodeTable = new inode_t[super.num_inodes];
  readInodeRegion(&super, inodeTable);
  inode_t newInode;
  newInode.type = type;
  newInode.size = 0;
  inodeTable[newInodeNumber] = newInode;
  writeInodeRegion(&super, inodeTable);
  delete[] inodeTable;

  // write to data blocks pointed to by newInode
  int writeDone = -1;
  if (type == UFS_DIRECTORY) { // newInode is a directory
    dir_ent_t curr; // "."
    dir_ent_t parent; // ".."
    memset(&curr, 0, sizeof(dir_ent_t)); // initialize all 0
    memset(&parent, 0, sizeof(dir_ent_t)); // initialize all 0
    curr.name[0] = '.'; 
    curr.inum = newInodeNumber;
    parent.name[0] = '.'; 
    parent.name[1] = '.';
    parent.inum = parentInodeNumber;
    dir_ent_t newDirTable[] = {curr, parent};
    writeDone = writeDataBlock(this, newInodeNumber, newDirTable, sizeof(newDirTable));
    if (writeDone < 0) { // writing blocks failed
      setBitAvailable(newInodeNumber, inodeBitmap); // set newInodeNumber as available again
      writeInodeBitmap(&super, inodeBitmap); // update bitmap
      delete[] inodeBitmap;
      return -ENOTENOUGHSPACE;
    }
  } // else, newInode is a regular file, no need to update data blocks

  // add new dir entry to existing dirTable pointed to by parentInode
  int dirTableSize = parentInode.size / sizeof(dir_ent_t);
  dir_ent_t dirTable[dirTableSize + 1]; // 1 extra for new
  getDirTable(parentInode, disk, dirTableSize, dirTable);
  dir_ent_t newDirEnt;
  memset(&newDirEnt, 0, sizeof(dir_ent_t)); // initialize all 0
  memcpy(newDirEnt.name, name.c_str(), name.size() + 1); // copy string name
  newDirEnt.inum = newInodeNumber; // set to new inode number
  dirTable[dirTableSize] = newDirEnt; // put new entry at end 

  // write updated parent dir back to disk
  writeDone = writeDataBlock(this, parentInodeNumber, dirTable, sizeof(dirTable));

  // if data blocks full, free allocated inodes before error
  if (writeDone < 0) {
    setBitAvailable(newInodeNumber, inodeBitmap); // set newInodeNumber as available again
    writeInodeBitmap(&super, inodeBitmap); // update bitmap
    delete[] inodeBitmap;
    return -ENOTENOUGHSPACE;
  }

  delete[] inodeBitmap;
  return newInodeNumber;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  return 0;
}

