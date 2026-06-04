#include <iostream>
#include <string>
#include <vector>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;
// creates new file
// takes disk file image, parentInode for directory the new entry is made, name of new entry

int main(int argc, char *argv[]) {
  if (argc != 4) {
    cerr << argv[0] << ": diskImageFile parentInode fileName" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " a.img 0 a.txt" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int parentInodeNumber = stoi(argv[2]);
  string fileName = string(argv[3]);

  super_t super;
  fileSystem->readSuperBlock(&super);
  inode_t parentInode;

  if (fileSystem->stat(parentInodeNumber, &parentInode) < 0) { // error
    cerr << "Error creating file" << endl;
    delete fileSystem;
    delete disk;  
    return 1;
  }

  if (parentInode.type != UFS_DIRECTORY) { // check if parent inode is a directory 
    cerr << "Error creating file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }

  int temp = fileSystem->create(parentInodeNumber, UFS_REGULAR_FILE, fileName); // attempt to make new file
  if (temp < 0) { // error
    cerr << "Error creating file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }
  
  delete fileSystem;
  delete disk;
  return 0;
}
