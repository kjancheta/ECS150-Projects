#include <iostream>
#include <vector>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

// we're going to ignore error checking to keep this
// example simple, but make sure that you do full
// error checking in any code that you write for
// projects in this class and in general if you want
// to actually use the software.

//lecture 4-13-26
// crazy cat takes list of all files on command line

int main(int argc, char *argv[]) {
    vector<int> fdVec; // vector of integers for file descriptor
    fdVec.push_back(STDIN_FILENO);

    for (int idx = 1; idx < argc; idx++){
        int fd = open(argv[idx], O_RDONLY);
        fdVec.push_back (fd);
    }

    for (int idx = 0; idx < fdVec.size(); idx++) {
        if (fork() == 0) {
            //child
            int fd = fdVec[idx];
            char buffer[4096];
            int ret;

            while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
                write(STDOUT_FILENO, buffer, ret);
            }
            close(fd); // if dont return, executes again in for loop with 2 parents, keeps going, runaway process
            return 0;
        } else {
            // parent
        }
    }

    // fdVec.size() equal to number of children we have
    for (int idx = 0; idx < fdVec.size(); idx++) {
        wait(NULL); // waits until a child process is done, then continues executing
    }

    cout << "all done!" << endl;

    return 0;
}