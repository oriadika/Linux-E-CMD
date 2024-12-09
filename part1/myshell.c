#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <string.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "LineParser.h"

#define Max_String 2048

int debug_mode = 0;
FILE *outfile;

void execute(cmdLine *cmdLine);
void setDebugMode(int argc, char **argv);
int stoierr(char *str);
void handleExit(char *msg, int errKind, int toExit, int errCode, int exit_kind, cmdLine *cmdLine);
void handleCWD();
cmdLine *handleInputAndParse();
int validCmdLine(cmdLine *cmdLine);
int handleSpecialCommand(cmdLine *cmdLine);
void handleSignalCommand(cmdLine *cmdLine);
void handleRedirection(const char *filePath, int targetFd, int flags, cmdLine *pCmdLine);

int main(int argc, char **argv)
{
    cmdLine *cmdLine;
    setDebugMode(argc, argv);

    while (1)
    {
        handleCWD();
        cmdLine = handleInputAndParse();
        if (!validCmdLine(cmdLine) || !handleSpecialCommand(cmdLine))
            continue;

        handleSignalCommand(cmdLine);
    }
    return 0;
}

// Execute the command
void execute(cmdLine *pCmdLine)
{
    int temp, stat_loc;
    int ProcessID = fork();

    if (ProcessID == -1)
        handleExit("Error in Forking: ", 1, 1, 1, 0, pCmdLine);

    // Child Process
    if (ProcessID == 0)
    {
        if (debug_mode)
            fprintf(stderr, "PID: %d\n  Executing Command: %s\n\n", ProcessID, pCmdLine->arguments[0]);

        // Handle redirections
        handleRedirection(pCmdLine->inputRedirect, STDIN_FILENO, O_RDONLY | O_CREAT, pCmdLine);
        handleRedirection(pCmdLine->outputRedirect, STDOUT_FILENO, O_WRONLY | O_CREAT, pCmdLine);

        // Check if the command is executable
        if (execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1)
            handleExit("Error in executing the command", 1, 1, EXIT_FAILURE, 0, pCmdLine);
    }

    // Parent Process.. Wait for the child to finish ProcessID>0
    else if (pCmdLine->blocking && waitpid(ProcessID, &stat_loc, 0) == -1)
        handleExit("Error in waiting for the child process", 1, 0, 0, 0, pCmdLine);
}

// Debug Mode Function - Looking for -d or -D
void setDebugMode(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if ((strcmp(argv[i], "-D") || !strcmp(argv[i], "-d")) && !debug_mode)
            debug_mode = 1;

    outfile = debug_mode ? stderr : stdout;
}

// Convert String to Integer
int stoierr(char *str)
{
    int val;
    // Check if the conversion was successful, if not print an error message and exit
    if (!sscanf(str, "%d", &val))
        handleExit("Error in converting string to integer", 1, 1, EXIT_FAILURE, 1, NULL);

    return val;
}

/**

 *  Handle Exit Function
 *
 *  msg - Error Message
 *  errKind - Error Kind (1 for perror, 0 for fprintf)
 *  toExit - Exit the program or not
 *  errCode - Error Code
 *  cmdLine - Command Line to free
 *  exit_kind - 0 for _exit, 1 for exit
*/
void handleExit(char *msg, int errKind, int toExit, int errCode, int exit_kind, cmdLine *cmdLine)
{
    if (msg != NULL)
        if (errKind)
            perror(msg);
        else
            fprintf(stderr, "%s\n", msg);

    if (cmdLine)
        freeCmdLines(cmdLine);

    if (toExit)
        if (exit_kind)
            exit(errCode);
        else
            _exit(errCode);
}

// Handle the Current Working Directory
void handleCWD()
{
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd)))
        handleExit("getcwd failed", 1, 1, 1, 1, NULL);

    sleep(1);
    printf("$%s> ", cwd);
}

// Handle the Input and Parse it
cmdLine *handleInputAndParse()
{
    char input[Max_String]; // Buffer
    if (!fgets(input, sizeof(input), stdin))
        handleExit("fgets failed", 1, EXIT_FAILURE, EXIT_FAILURE, 1, NULL);

    return parseCmdLines(input); // Parse the input
}

// Check if the Command Line is Valid
int validCmdLine(cmdLine *cmdLine)
{
    if (!cmdLine)
    {
        if (feof(stdin))
            exit(0);

        fprintf(stderr, "Error: got null command\n");
        return 0;
    }

    if (!cmdLine->argCount)
        handleExit("NO given arguments in the Command", 0, 0, 0, 1, cmdLine);

    return 1;
}

/**
 * Handle Special Commands
 * return 0 if should continue, 1 if ok
 */
int handleSpecialCommand(cmdLine *cmdLine)
{
    if (strcmp(cmdLine->arguments[0], "quit") == 0)
        handleExit(NULL, 0, 1, 0, 1, cmdLine);

    if (strcmp(cmdLine->arguments[0], "cd") == 0)
    {
        if (chdir(cmdLine->arguments[1]) == -1)
            fprintf(stderr, "Error in changing the directory");

        freeCmdLines(cmdLine);
        return 0;
    }
    return 1;
}

void handleSignalCommand(cmdLine *cmdLine)
{
    int cmdNo = 0;
    int processID;

    if (!strcmp(cmdLine->arguments[0], "stop"))
        cmdNo = SIGSTOP;
    else if (!strcmp(cmdLine->arguments[0], "wake"))
        cmdNo = SIGCONT;
    else if (!strcmp(cmdLine->arguments[0], "term"))
        cmdNo = SIGINT;

    else // not a special command
    {
        execute(cmdLine);
        freeCmdLines(cmdLine);
        cmdLine = NULL;
        return;
    }

    if (cmdNo) // a special command
        if (cmdLine->argCount == 2)
        {
            processID = stoierr(cmdLine->arguments[1]);
            if (processID != -1)
                kill(processID, cmdNo);
        }
        else
            fprintf(stderr, "Error in the number of arguments\n");
    // else
    //     printf("Error in the command\n");
}

void handleRedirection(const char *filePath, int targetFd, int flags, cmdLine *pCmdLine)
{
    if (filePath)
    {
        close(targetFd);
        if (open(filePath, flags, 0777) == -1)
            handleExit("Error in opening the file", 1, 1, EXIT_FAILURE, 0, pCmdLine);
    }
}