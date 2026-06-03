#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;
// prints metadata for the file system on a disk image. 
// It takes a single command line argument: the name of a disk image file.

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  // Parse command line arguments
  
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);

  super_t* super = new super_t;
  fileSystem->readSuperBlock(super);

  cout << "Super" << endl;
  cout << "inode_region_addr " << super->inode_region_addr << endl;
  cout << "inode_region_len " << super->inode_region_len << endl; 
  cout << "num_inodes " << super->num_inodes << endl;
  cout << "data_region_addr " << super->data_region_addr << endl;
  cout << "data_region_len " << super->data_region_len << endl;
  cout << "num_data " << super->num_data << endl;
  cout << endl;

  // inode bitmap
  unsigned char inodeBitmap[(super->inode_bitmap_len) * UFS_BLOCK_SIZE]; // to hold inodebitmap region
  fileSystem->readInodeBitmap(super, inodeBitmap); // read bitmap data
  cout << "Inode bitmap" << endl;
  for (int i = 0; i < (super->num_inodes)/8; i++) { // loop and print bitmap btyes
    cout << (unsigned int) inodeBitmap[i] << " ";
  }
  cout << endl << endl;
  // data bitmap
  unsigned char dataBitmap[(super->data_bitmap_len) * UFS_BLOCK_SIZE]; // to hold databitmap region
  fileSystem->readDataBitmap(super, dataBitmap); // read bitmap data
  cout << "Data bitmap" << endl;
  for (int i = 0; i < (super->num_data)/8; i++) { // loop and print bitmap btyes
    cout << (unsigned int) dataBitmap[i] << " ";
  }
  cout << endl;

  delete super;
  delete fileSystem;
  delete disk;
  
  return 0;
}
