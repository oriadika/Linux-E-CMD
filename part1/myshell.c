#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "LineParser.h"

#define HISTLEN 10
#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

// Process structure
typedef struct process {
    cmdLine *cmd;
    pid_t pid;
    int status;
    struct process *next;
} process;

process *processList = NULL;  // Global process list
char *history[HISTLEN];       // History queue
int historyCount = 0;         // History counter

// Function prototypes
void executeCommand(cmdLine *cmd);
void executePipeline(cmdLine *cmd);
void addProcess(process **processList, cmdLine *cmd, pid_t pid);
void printProcessList(process **processList);
void updateProcessList(process **processList);
void signalProcess(process **processList, pid_t pid, int signal);
void addHistory(char *cmd);
void printHistory();
char *getHistoryCommand(int index);

// Main shell loop
int main() {
    char input[1024]; // Buffer for user input

    while (1) {
        // Prompt
        printf("myshell> ");
        if (!fgets(input, sizeof(input), stdin)) {
            break; // Exit loop if EOF (Ctrl+D) or error occurs
        }

        if (strlen(input) <= 1) {
            continue; // Ignore empty input
        }

        // Add the command to history
        addHistory(input);

        // Parse the command line
        cmdLine *cmd = parseCmdLines(input);
        if (!cmd) {
            continue; // Skip invalid input
        }

        // Built-in commands handling
        if (strcmp(cmd->arguments[0], "history") == 0) {
            printHistory();
        } else if (strcmp(cmd->arguments[0], "!!") == 0) {
            char *lastCommand = getHistoryCommand(historyCount); // Get last command
            if (lastCommand) {
                cmdLine *lastCmd = parseCmdLines(lastCommand);
                if (lastCmd) {
                    executeCommand(lastCmd); // Execute the last command
                    freeCmdLines(lastCmd);
                } else {
                    fprintf(stderr, "Error: Failed to parse last command.\n");
                }
            } else {
                fprintf(stderr, "Error: No previous command found.\n");
            }
        } else if (cmd->arguments[0][0] == '!') {
            int index = atoi(&cmd->arguments[0][1]); // Extract index from !n
            char *historyCommand = getHistoryCommand(index);
            if (historyCommand) {
                cmdLine *histCmd = parseCmdLines(historyCommand);
                if (histCmd) {
                    executeCommand(histCmd); // Execute the command at index
                    freeCmdLines(histCmd);
                } else {
                    fprintf(stderr, "Error: Failed to parse command at history index %d.\n", index);
                }
            } else {
                fprintf(stderr, "Error: No command found at index %d.\n", index);
            }
        } else if (strcmp(cmd->arguments[0], "procs") == 0) {
            printProcessList(&processList);
        } else if (strcmp(cmd->arguments[0], "stop") == 0 && cmd->argCount > 1) {
            pid_t pid = atoi(cmd->arguments[1]);
            signalProcess(&processList, pid, SIGTSTP);
        } else if (strcmp(cmd->arguments[0], "wake") == 0 && cmd->argCount > 1) {
            pid_t pid = atoi(cmd->arguments[1]);
            signalProcess(&processList, pid, SIGCONT);
        } else if (strcmp(cmd->arguments[0], "term") == 0 && cmd->argCount > 1) {
            pid_t pid = atoi(cmd->arguments[1]);
            signalProcess(&processList, pid, SIGINT);
        } else {
            executeCommand(cmd); // Execute external commands or pipelines
        }

        // Free command line structure
        freeCmdLines(cmd);
    }

    return 0;
}


// Execute a single command
void executeCommand(cmdLine *cmd) {
    if (cmd->next) {
        executePipeline(cmd);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) { // Child process
        if (cmd->inputRedirect) {
            freopen(cmd->inputRedirect, "r", stdin);
        }
        if (cmd->outputRedirect) {
            freopen(cmd->outputRedirect, "w", stdout);
        }

        execvp(cmd->arguments[0], cmd->arguments);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    if (cmd->blocking) {
        waitpid(pid, NULL, 0);
    } else {
        addProcess(&processList, cmd, pid);
    }
}

// Execute a pipeline of two commands
void executePipeline(cmdLine *cmd) {
    int pipefd[2];           // Pipe file descriptors
    int inputFd = 0;         // Input for the next command
    pid_t pid;

    while (cmd->next) {
        pipe(pipefd);        // Create a new pipe

        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { // Child process
            dup2(inputFd, STDIN_FILENO);  // Set the input for the current process
            dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
            close(pipefd[0]); // Close unused read end
            execvp(cmd->arguments[0], cmd->arguments);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else {
            waitpid(pid, NULL, 0); // Parent waits for the child
            close(pipefd[1]);      // Close unused write end
            inputFd = pipefd[0];   // Save read end for the next command
            cmd = cmd->next;       // Move to the next command in the pipeline
        }
    }

    // Last command in the pipeline
    if ((pid = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        dup2(inputFd, STDIN_FILENO); // Set input for the final process
        execvp(cmd->arguments[0], cmd->arguments);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        waitpid(pid, NULL, 0); // Parent waits for the final child
    }
}


// Add a process to the process list
void addProcess(process **processList, cmdLine *cmd, pid_t pid) {
    process *newProcess = malloc(sizeof(process));
    newProcess->cmd = cmd;
    newProcess->pid = pid;
    newProcess->status = RUNNING;
    newProcess->next = *processList;
    *processList = newProcess;
}

// Print the process list
void printProcessList(process **processList) {
    process *p = *processList;
    printf("PID\t\tCommand\t\tStatus\n");
    while (p) {
        printf("%d\t\t%s\t\t%s\n", p->pid, p->cmd->arguments[0],
               p->status == RUNNING ? "Running" : p->status == SUSPENDED ? "Suspended" : "Terminated");
        p = p->next;
    }
}

// Signal a process in the process list
void signalProcess(process **processList, pid_t pid, int signal) {
    process *p = *processList;
    while (p) {
        if (p->pid == pid) {
            if (kill(pid, signal) == 0) {
                if (signal == SIGTSTP)
                    p->status = SUSPENDED;
                else if (signal == SIGCONT)
                    p->status = RUNNING;
                else if (signal == SIGINT)
                    p->status = TERMINATED;
            } else {
                perror("kill");
            }
            return;
        }
        p = p->next;
    }
    fprintf(stderr, "No process with PID %d found.\n", pid);
}

// Add a command to history
void addHistory(char *cmd) {
    // Trim trailing newline
    size_t len = strlen(cmd);
    if (len > 0 && cmd[len - 1] == '\n') {
        cmd[len - 1] = '\0';
    }

    if (historyCount == HISTLEN) {
        free(history[0]);
        for (int i = 1; i < HISTLEN; i++) {
            history[i - 1] = history[i];
        }
        historyCount--;
    }
    history[historyCount++] = strdup(cmd);
}

// Get a command from history
char *getHistoryCommand(int index) {
    if (index < 1 || index > historyCount) {
        fprintf(stderr, "Invalid history index.\n");
        return NULL;
    }
    return history[index - 1];
}

void printHistory() {
    for (int i = 0; i < historyCount; i++) {
        printf("%d %s\n", i + 1, history[i]); // Print command index and command string
    }
}
