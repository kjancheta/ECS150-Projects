#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>

using namespace std;

// wgrep looks through a file line by line to find a user-specified search term
// if a line has the word (case sensitive), it is printed out, if not it is skipped

int main(int argc, char *argv[]) {
    int fd;
    if (argc == 1) { // no arguments specified on command line
        const char msg[] = "wgrep: searchterm [file ...]\n"; // error message from README
        write(STDOUT_FILENO, msg, sizeof(msg) - 1); // print with write
        return 1; // just exit and return 1
    }

    if (argc == 2) { // search term specified but no file
        fd = STDIN_FILENO; // read from standard input instead
    }

    char *search = argv[1]; // search term argument (ie foo)
    char buffer[1048576]; // buffers data until newline character, one of the tests was a really long line
    int lineIdx = 0; // index for buffer
    char bytesChunk[4096]; // reads large chunks and stores, was too long reading 1 at a time so this helped
    int ret;

    for (int i = 2; i < argc || (argc == 2 && i == 2); i++) { // start at 2 for argv[2] file name
                                                              // argc = 2 case is for no file
        if (argc > 2) { // there are files on the command line
            fd = open(argv[i], O_RDONLY);
            if (fd < 0) { // negative number returned, open() failed
                const char msg[] = "wgrep: cannot open file\n"; // error message from README
                write(STDOUT_FILENO, msg, sizeof(msg) - 1); 
                return 1; // exit with status code 1
            }
        }

        lineIdx = 0; // line index reset for start of file

        while ((ret = read(fd, bytesChunk, 4096)) > 0) { // reads up to 4096 bytes at a time 
            for (int j = 0; j < ret; j++) { // iterate through each byte in chunk
                char c = bytesChunk[j]; // store 1 character from chunk
                buffer[lineIdx] = c; // add character to buffer
                lineIdx++;
                if (c == '\n') { // encountered newline character, end of line
                    buffer[lineIdx] = '\0';
                    if (strstr(buffer, search)) { // search for the term in line
                        write(STDOUT_FILENO, buffer, lineIdx); // prints line 
                    }
                    lineIdx = 0; // reset for new line
                }
            }
        }

        if (argc > 2) {
            close(fd); 
        }
    }

    return 0;
}