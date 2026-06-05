#include <iostream>
#include <string>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;
// copies a file from your computer into the disk image using your `LocalFileSystem.cpp` implementation
// takes three command line arguments, the disk image file, the source file from your
  // computer that you want to copy in, and the inode for the destination file within your disk image
bool bitAvailable(int bitPos, unsigned char* bitmap);

int main(int argc, char *argv[]) {
  if (argc != 4) {
    cerr << argv[0] << ": diskImageFile src_file dst_inode" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img dthread.cpp 3" << endl;
    return 1;
  }

  // Parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string srcFile = string(argv[2]);
  int dstInodeNumber = stoi(argv[3]);
  
  // read file from local computer
  int fd = open(srcFile.c_str(), O_RDONLY); // open in read only
  if (fd < 0) { // error 
    cerr << "Could not write to dst_file" << endl;
    delete fileSystem;
    delete disk;  
    return 1;
  }

  off_t fileSize = lseek(fd, 0, SEEK_END); // move offset pointer to end to calculate size in bytes
  lseek(fd, 0, SEEK_SET); // reset offset pointer

  super_t super;
  fileSystem->readSuperBlock(&super);
  inode_t dstInode;
  int resultStat = fileSystem->stat(dstInodeNumber, &dstInode);
  if (resultStat < 0) { // error
    cerr << "Could not write to dst_file" << endl;
    delete fileSystem;
    delete disk;  
    return 1;
  }

  int inodeBitmapSize = super.inode_bitmap_len * UFS_BLOCK_SIZE;
  unsigned char* inodeBitmap = new unsigned char[inodeBitmapSize];
  fileSystem->readInodeBitmap(&super, inodeBitmap);
  bool inodeNumberAvailable = bitAvailable(dstInodeNumber, inodeBitmap); // check if inodeNumber is available

  if ((dstInode.type == UFS_DIRECTORY) || inodeNumberAvailable) { // error, trying to write to directory or free inode
    cerr << "Could not write to dst_file" << endl;
    delete fileSystem;
    delete disk;  
    delete[] inodeBitmap;
    return 1;
  }

  // write to destination
  int bytesRead = 0; 
  int totalBytesRead = 0;
  char buffer[4096];
  char* dataBuffer = new char[fileSize];
  int idx = 0;
  while (1) { // loop to read data from file
    bytesRead = read(fd, buffer, sizeof(buffer));
    totalBytesRead += bytesRead;
    memcpy(dataBuffer + idx * 4096, buffer, bytesRead); // copy into buffer
    idx++;
    if (bytesRead < 4096) {
      break;
    }
  }

  int bytesWrote = fileSystem->write(dstInodeNumber, dataBuffer, totalBytesRead); 
  if (bytesWrote < 0) { // error
    cerr << "Could not write to dst_file" << endl;
    delete fileSystem;
    delete disk;  
    delete[] inodeBitmap;
    delete[] dataBuffer;
    return 1;
  }

  close(fd); // close file

  delete fileSystem;
  delete disk;
  delete[] inodeBitmap;
  delete[] dataBuffer;
  return 0;
}
