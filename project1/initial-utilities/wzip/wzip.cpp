#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>

using namespace std;

// wzip compresses files using run-length encoding (RLE)
// encountering n characters of the same type in a row, 
// turn n into a number and single instance of that character
// ex aaaaaaaaaabbbb -> 10a4b
// format as 4-byte int in binary followed by 1 ascii character
// compressed file has some number of 5-byte entries

int main(int argc, char *argv[]) {
    int fd; 
    if (argc == 1) { // no arguments specified on command line
        const char msg[] = "wzip: file1 [file2 ...]\n"; // error message from README
        write(STDOUT_FILENO, msg, sizeof(msg) - 1); // print with write
        return 1; // just exit and return 1
    }

    char bytesChunk[4096]; // reads large chunks and stores
    int count = 0; // number of instances of a char
    char currentChar = 0; // holds the current char being counted
    int ret;

    for (int i = 1; i < argc; i++) { 
        fd = open(argv[i], O_RDONLY); 
        if (fd < 0) { // negative number returned, open() failed
            const char msg[] = "wzip: cannot open file\n"; // error message from README
            write(STDOUT_FILENO, msg, sizeof(msg) - 1); // print with write
            return 1; // exit with status code 1
        }

        while ((ret = read(fd, bytesChunk, 4096)) > 0) { // reads up to 4096 bytes at a time 
            for (int j = 0; j < ret; j++) { // iterate through each byte in chunk
                char c = bytesChunk[j]; // store 1 character from chunk
                if (count == 0) { // for first character
                    currentChar = c;
                    count = 1;
                }
                else if (currentChar == c) { // new c found is same as char being counted
                    count++; 
                }
                else { // new c found is different from char being counted
                    write(STDOUT_FILENO, &count, 4); // prints 4 byte count
                    write(STDOUT_FILENO, &currentChar, 1); // prints 1 byte char
                    currentChar = c; // start counting new char
                    count = 1; // reset count
                }

            }
        }

        close(fd);
        
    }

    // the for loop prints when a different character is found
    // still need to print the last variables
    if (count > 0) {
        write(STDOUT_FILENO, &count, 4);
        write(STDOUT_FILENO, &currentChar, 1);
    }

    return 0;
}