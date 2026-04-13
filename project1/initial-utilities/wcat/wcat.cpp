#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace std;

// wcat reads main.cpp and prints its contents
// from readme

int main(int argc, char *argv[]) {
    int fd; // file descriptor
    if (argc == 1) { // no files specified on command line
        return 0; // just exit and return 0
    }

    char buffer[4096];
    int ret;

    for (int i = 1; i < argc; i++) {
        fd = open(argv[i], O_RDONLY); // open() tries opening file and returns a file descriptor,
                                      // pass O_RDONLY to read the file
        if (fd < 0) { // negative number returned, open() failed
            const char msg[] = "wcat: cannot open file\n"; // error message from README
            write(STDOUT_FILENO, msg, sizeof(msg) - 1); // print with write, ignore ending \0
            return 1; // exit with status code 1
        }

        while ((ret = read(fd, buffer, 4096)) > 0) { // while not end of file
            write(STDOUT_FILENO, buffer, ret); // write ret bytes from buffer 
        }

        close(fd); // done reading and writing, close file
    }

    return 0;
}