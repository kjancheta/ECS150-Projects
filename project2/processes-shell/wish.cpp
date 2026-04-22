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
    // format of redirection is a command (maybe some args) followed by > followed by filename
        // so no command before > is an error
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

// splits up command line input
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
            // add current arg first then take symbol
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

// find executable in path
string findExec(string line, vector<string> path) {
    for (int i = 0; i < path.size(); i++) {
        string searchPath = path[i] + "/" + line; // build search path
        if (access(searchPath.c_str(), X_OK) == 0) { // 0 means its execute permission is enabled
            return searchPath; // found
        }
    }
    return ""; // not found
}

// checks for >
string checkRedirect(vector<string> &args) { // & to change it in main
    string file = "";
    for (int i = 0; i < args.size(); i++) {
        if (args[i] == ">") {
            // nothing after > or multiple files after > (and multiple > symbols)
            if (i + 1 != args.size() - 1) { // index after > is not the last arg
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
                return "uhoh";
            }
            // nothing before >
            if (i == 0) { // i = 0, then > is first (no command before it)
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
                return "uhoh";
            }
            file = args[i + 1]; // gets the file after >
            args.erase(args.begin() + i, args.end()); // removes everything from > to end so execv works
            return file;
        }
    }
    return "";
}

// checks for & to split into multiple commands
// parallel commands, need multiple vectors
vector<vector<string>> checkParallel(vector<string> args) { 
    vector<vector<string>> commands; // stores commands to be run in parallel
    vector<string> currentCmd; // command being currently built
    for (int i = 0; i < args.size(); i++) {
        if (args[i] == "&") {
            if (!currentCmd.empty()) {
                commands.push_back(currentCmd); // store command to list of commands
                currentCmd.clear(); // build next command
            }
        }
        else {
            currentCmd.push_back(args[i]); // build current command
        }
    }
    if (!currentCmd.empty()) { // store final command
        commands.push_back(currentCmd);
    }
    return commands;
}

int main(int argc, char *argv[]) {
    string line;
    vector<string> shellPath = {"/bin"}; // initial shell path, only one directory initially

    if (argc > 2) { // error, invoked with multiple files
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    // check for batch mode
    if (argc == 2) {
        int fd = open(argv[1], O_RDONLY);
        if (fd < 0) { // bad batch file
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    while (true) {
        if (argc == 1) {
            cout << "wish> "; // prompt only in interactive mode, not batch
        }
        if (!getline(cin, line)) { 
            exit(0); // EOF
        }

        // parse the command line
        vector<string> args = parseCommand(line); 
        
        if (args.empty()) {
            continue; // blank, prompt again
        }

        // exit built in
        if (args[0] == "exit") { 
            if (args.size() > 1) { // args passed to 'exit', error
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
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
        if (args[0] == "path") {
            shellPath.clear(); // clear shellPath to overwrite it
            for (int i = 1; i < args.size(); i++) {
                shellPath.push_back(args[i]); // add each arg as the new path
            }
            continue;
        }

        // done checking built ins
        // parallel commands and execute
        vector<vector<string>> commands = checkParallel(args);
        vector<pid_t> pids; // store child pids
        for (int i = 0; i < commands.size(); i++) {
            vector<string> currentCmd = commands[i];

            // check redirection
            string fileName = checkRedirect(currentCmd);
            if (fileName == "uhoh") {
                continue; // prompt again, error with redirect
            }

            // search for executables
            string executable = findExec(currentCmd[0], shellPath);
            if (executable.empty()) {
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue; // prompt again
            }

            // execute command
            pid_t pid = fork();
            if (pid == 0) {
                // child process
                // redirection
                if (fileName != ""){
                    // need to write to a file, overwrite if exists, create if not
                    // need user to read/write
                    int fd = open(fileName.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
                    if (fd < 0) { // open failed
                        char error_message[30] = "An error has occurred\n";
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        exit(1); // end child process
                    }
                    dup2(fd, STDOUT_FILENO); // stdout rerouted to file
                    dup2(fd, STDERR_FILENO); // stderr rerouted to file
                    close(fd);
                }
                // continue with execution
                char *execArgs[currentCmd.size() + 1]; // execv uses char* array
                for (int i = 0; i < currentCmd.size(); i++) {
                    execArgs[i] = (char *)currentCmd[i].c_str(); // convert string to char*
                }
                execArgs[currentCmd.size()] = NULL; // for execv, array must be terminated by null
                execv(executable.c_str(), execArgs); // replace child process w command
                // execv only returns when there is an error
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1); // end child process
            }
            else if (pid > 0) {
                // parent process
                pids.push_back(pid); // save pid
            }
            else {
                // fork failed
                char error_message[30] = "An error has occurred\n";
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
        // all processes started, now wait
        for (int i = 0; i < pids.size(); i++) {
            wait(NULL);
        }
    }

    return 0;
}

