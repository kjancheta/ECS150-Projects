#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>

using namespace std;

// wunzip does the opposite of zip,
// ex 10a4b -> aaaaaaaaaabbbb
// format as 4-byte int in binary followed by 1 ascii character
// compressed file has some number of 5-byte entries
// so for wunzip, first read 4 for the count, then 1 for the character

int main(int argc, char *argv[]) {
    int fd; 
    if (argc == 1) { // no arguments specified on command line
        const char msg[] = "wunzip: file1 [file2 ...]\n"; // error message from README
        write(STDOUT_FILENO, msg, sizeof(msg) - 1); // print with write
        return 1; // just exit and return 1
    }

    int count = 0; // number of instances of a char to be written, 4 bytes
    char currentChar = 0; // holds the char linked with the count, 1 byte

    for (int i = 1; i < argc; i++) { 
        fd = open(argv[i], O_RDONLY); 
        if (fd < 0) { // negative number returned, open() failed
            const char msg[] = "wunzip: cannot open file\n"; // error message from README
            write(STDOUT_FILENO, msg, sizeof(msg) - 1); // print with write
            return 1; // exit with status code 1
        }

        while (read(fd, &count, 4) == 4) { // reads 4 bytes for the count
            read(fd, &currentChar, 1); // reads 1 byte for the character 
            for (int j = 0; j < count; j++) { // write character 'count' times
                write(STDOUT_FILENO, &currentChar, 1); // print the char
            }
        }

        close(fd); 
    }   

    return 0;
}