#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <string>

using namespace std;

// readme summary :D
// indefinite while loop until user types 'exit' command
    // prints a prompt 'wish> '
    // shell can be invoked w 0 or 1 argument, else is error
// getline() to read lines of input
// interactive mode (user types command) and batch mode (input file)
    // in both modes, call exit(0) at end of file
    // batch invoked with ./wish batch.txt
// use stl::string class to parse input line
// to execute commands, fork(), exec(), wait()/waitpid()
// USE execv() (returns if error), DO NOT USE system()
// user specifies a path variable, list of directories to search in order
// * shell doesnt implement commands except built ins
    // finds the executables in one directory specified by path
    // and makes a process to run them
// to check if file exists in directory and is executable, use access()
// initial shell path contains 1 directory, '/bin'
    // when 'path' command invoked, replaces path w arguments that user passes
// when a command is accepted, check if it is built in
    // if yes, invoke your implementation of the built in command
        // to implement 'exit' built in command, call 'exit(0);'
    // 'exit', 'cd', and 'path' are built in, need implementation
        // exit: user types exit, call exit sys call w 0 as parameter
            // passing args to it is an error
        // cd: takes 1 arg (0 or >1 args are an error)
            // to change directories, use 'chdir()' sys call
            // chdir fail is an error
        // path: takes 0 or more args, args separated by whitespace
            // 'wish> path /bin /usr/bin', would set '/bin' and '/usr/bin' 
            // as search path of the shell
            // if path is empty, shall can only run built in commands
            // path command overwrites old path
// redirect: reroute output (and error) to the '_' file instead of printing
    // if a user types 'ls -la /tmp > output', redirect to 'output' file
    // if the file exists, overwrite it
    // multiple redirect operators or multiple files to right
        // of redirection sign are errors
// run parallel commands with & symbol
    // wish> cmd1 & cmd2 args1 args2 & cmd3 args1
        // runs cm1 cm2 and cm3 in parallel, not waiting for 1 to finish
    // after starting all processes, use wait() or waitpid() 
// error: only print 1 error message for any type
    // char error_message[30] = "An error has occurred\n";
    // write(STDERR_FILENO, error_message, strlen(error_message)); 
    // print to stderr
    // after error, shell should continue processing
        // but if it has >1 files or is passed a bad batch file, call exit(1)
    // catch all syntax errors
// to manage processes, use the Process API calls:
    // fork
    // execv
    // dup2 (also required for redirection)
    // one of wait family system call
// order to implement:
    // basic functionality (single command running like 'ls'), built in,
    // redirection, parallel commands, check whitespaces (?)

vector<string> parseCommand (string line) {
    vector<string> args; // vector of the args of the command line
    string arg;
    // use stl::string class to parse
    for (int i = 0; i <= line.size(); i++) {
        char c;
        if (i == line.size()) { // to add final arg
            c = ' '; // treat c as whitespace
        }
        else {
            c = line[i]; // c is current char
        }

        if (c == ' ' || c == '\t') { // spaces and tabs
            if (!arg.empty()) {
                args.push_back(arg);
                arg = "";
            }
        }
        // handle redirection or parallel symbols
        else if (c == '>' || c == '&'){ 
            // add current arg to first then take symbol
            if (!arg.empty()) {
                args.push_back(arg);
                arg = "";
            }
            args.push_back(string(1, c)); // add symbol by itself to args
        }
        else {
            arg += c; // add on to current arg
        }
    }
    return args;
}

string findExec(string line, vector<string> path) {
    for (int i = 0; i < path.size(); i++) {
        string searchPath = path[i] + "/" + line; // build search path
        if (access(searchPath.c_str(), X_OK) == 0) { // 0 means its execute permission is enabled
            return searchPath; // found
        }
    }
    return ""; // not found
}

int main(int argc, char *argv[]) {
    string line;
    vector<string> shellPath = {"/bin"}; // initial shell path, only one directory

    while (true) {
        cout << "wish> "; // prompt
        if (!getline(cin, line)) { // REMOVE?
            exit(0);
        }

        vector<string> args = parseCommand(line); 
        
        if (args.empty()) {
            continue; // prompt again
        }

        // exit built in
        if (args[0] == "exit") { 
            if (args.size() > 1) { // args passed to 'exit', error!
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            else {
                exit(0);
            }
        }

        // cd built in
        if (args[0] ==  "cd") {
            if (args.size() != 2) { // cd must take 1 argument, 0 or more than 1 is error
                                    // size = 2, 1 for cd then 2 for directory
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            else {
                int chdirResult = chdir(args[1].c_str());
                if (chdirResult != 0) { // on error, -1 is returned (0 on success)
                    char error_message[30] = "An error has occurred\n";
                    write(STDERR_FILENO, error_message, strlen(error_message));
                }
            }
            continue;
        }


        // path built in

        string executable = findExec(args[0], shellPath);
        if (executable.empty()) {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue; // prompt again
        }

        // execute command
        pid_t pid = fork();
        if (pid == 0) {
            // child process
            char *execArgs[args.size() + 1]; // execv uses char* array
            for (int i = 0; i < args.size(); i++) {
                execArgs[i] = (char *)args[i].c_str(); // convert string to char*
            }
            execArgs[args.size()] = NULL; // for execv, array must be terminated by null

            execv(executable.c_str() ,execArgs); // replace child process w command
            // execv only returns when there is an error
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1); // end child process
        }
        else if (pid > 0) {
            // parent process
            wait(NULL); // wait for child
        }
        else {
            // fork failed
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
        }

    }

    return 0;
}

