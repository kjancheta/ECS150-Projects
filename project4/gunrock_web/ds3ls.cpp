#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sstream>

#include "StringUtils.h"
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;
// prints contents of a directory
// 2 args: name of disk image file and path of directory or file to list within the disk image
// for directories: prints all entries in the directory sorted using std::sort, each in its own line
// for file: prints just the information for that file, each entry includes inode #, tab, name of entry, endl


vector<string> parsePath(string path) {
  vector<string> pathVec;
  stringstream ss(path);
  string component;

  while (getline(ss, component, '/')) { // split string at '/' character
    pathVec.push_back(component);
  }
  pathVec.erase(pathVec.begin()); // remove extra empty part in front
  return pathVec;
}

// Use this function with std::sort for directory entries
bool compareByName(const dir_ent_t& a, const dir_ent_t& b) {
    return std::strcmp(a.name, b.name) < 0;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << argv[0] << ": diskImageFile directory" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
    return 1;
  }

  // parse command line arguments
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string directory = string(argv[2]);

  // traverse path
  vector<string> pathVec = parsePath(directory); // split path
  int pathInodeNumber = 0; // current inode number
  string componentName;
  while (!pathVec.empty()) { // find target inode number
    componentName = pathVec.front(); // get next in path
    pathVec.erase(pathVec.begin()); // remove it from vector

    pathInodeNumber = fileSystem->lookup(pathInodeNumber, componentName); // get the childs inode number using parent
    if (pathInodeNumber < 0) { // error
      cerr << "Directory not found" << endl;  
      delete fileSystem;
      delete disk;
      return 1;
    }
  }

  inode_t componentInode; // inode to hold data in final component
  int stat = fileSystem->stat(pathInodeNumber, &componentInode); // fill inode with data
  if (stat < 0) { // error
    cerr << "Directory not found" << endl;  
    delete fileSystem;
    delete disk;
    return 1;
  }

  if (componentInode.type == UFS_DIRECTORY) { 
    int dirTableSize = componentInode.size / sizeof(dir_ent_t); // number of directories
    dir_ent_t* dirTable = new dir_ent_t[dirTableSize];
    // read directory entries into dirTable
    int read = fileSystem->read(pathInodeNumber, reinterpret_cast<unsigned char*>(dirTable), componentInode.size);
    if (read < 0) { // error
      cerr << "Directory not found" << endl;  
      delete fileSystem;
      delete disk;
      delete[] dirTable;
      return 1;
    }

    // sort
    sort(dirTable, dirTable + dirTableSize, compareByName);

    // print
    for (int i = 0; i < dirTableSize; i++) {
      cout << dirTable[i].inum << "\t" << dirTable[i].name << endl;
    }
    delete[] dirTable;
  }
  else { // componentInode.type == UFS_REGULAR_FILE
    cout << pathInodeNumber << "\t" << componentName << endl;
  }

  delete fileSystem;
  delete disk;
  return 0;
}
