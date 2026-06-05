#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;
// removes a file or empty directory from your disk image's file system. 
// It takes three arguments: the disk image file name, the inode for the parent directory, and the name of the file or
  // directory that you want to delete.

int main(int argc, char *argv[]) {
  if (argc != 4) {
    cerr << argv[0] << ": diskImageFile parentInode entryName" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int parentInodeNumber = stoi(argv[2]);
  string entryName = string(argv[3]);
  
  int unlink = fileSystem->unlink(parentInodeNumber, entryName); // call unlink to remove
  if (unlink < 0) { // error
    cerr << "Error removing entry" << endl;
    delete fileSystem;
    delete disk;  
    return 1;
  }

  delete fileSystem;
  delete disk;  
  return 0;
}
