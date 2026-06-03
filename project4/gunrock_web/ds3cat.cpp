#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;
// prints the contents of a file to standard output. 
// It takes the name of the disk image file and an inode number as the only arguments. 
// It prints the contents of the file that is specified by the inode number.

int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << argv[0] << ": diskImageFile inodeNumber" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  int inodeNumber = stoi(argv[2]);
  
  inode_t contentInode; // inode to hold content
  int stat = fileSystem->stat(inodeNumber, &contentInode); // fill inode with data

  // check for error
  if ((stat < 0) || (contentInode.type == UFS_DIRECTORY)) { // if stat failed or inode is a directory
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk; 
    return 1;
  }

  // first print file blocks 
  cout << "File blocks" << endl;
  int blocks = contentInode.size / UFS_BLOCK_SIZE; // find # of blocks directory has
  if ((contentInode.size % UFS_BLOCK_SIZE) != 0) { // if not multiple of 4096, add a block
    blocks += 1;
  }
  for (int i = 0; i < blocks; i++) {
    cout << contentInode.direct[i] << endl; // print disk block numbers
  }

  cout << endl;

  // then print file data
  cout << "File data" << endl;
  unsigned char* fileContent = new unsigned char[contentInode.size]; // hold file contents
  int read = fileSystem->read(inodeNumber, fileContent, contentInode.size); // read data from disk into fileContent
  if (read < 0) { // error
    cerr << "Error reading file" << endl;
    delete[] fileContent;
    delete fileSystem;
    delete disk; 
    return 1;
  }
  for (int i = 0; i < contentInode.size; i++) {
    cout << fileContent[i]; // print contents
  }

  delete[] fileContent;
  delete fileSystem;
  delete disk;
  return 0;
}
