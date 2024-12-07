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
    char input[1024];

    while (1) {
        printf("myshell> ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        if (strlen(input) <= 1) {
            continue; // Skip empty input
        }

        addHistory(input);

        cmdLine *cmd = parseCmdLines(input);
        if (!cmd) {
            continue;
        }

        // Built-in commands
        if (strcmp(cmd->arguments[0], "history") == 0) {
            printHistory();
        } else if (strcmp(cmd->arguments[0], "!!") == 0) {
            char *lastCommand = getHistoryCommand(historyCount);
            if (lastCommand) {
                cmdLine *lastCmd = parseCmdLines(lastCommand);
                executeCommand(lastCmd);
                freeCmdLines(lastCmd);
            }
        } else if (cmd->arguments[0][0] == '!') {
            int index = atoi(&cmd->arguments[0][1]);
            char *historyCommand = getHistoryCommand(index);
            if (historyCommand) {
                cmdLine *histCmd = parseCmdLines(historyCommand);
                executeCommand(histCmd);
                freeCmdLines(histCmd);
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
            executeCommand(cmd);
        }

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
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    pid_t child1 = fork();
    if (child1 == -1) {
        perror("fork");
        return;
    }

    if (child1 == 0) { // First child
        close(STDOUT_FILENO);
        dup(pipefd[1]);
        close(pipefd[0]);
        close(pipefd[1]);

        execvp(cmd->arguments[0], cmd->arguments);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    pid_t child2 = fork();
    if (child2 == -1) {
        perror("fork");
        return;
    }

    if (child2 == 0) { // Second child
        close(STDIN_FILENO);
        dup(pipefd[0]);
        close(pipefd[1]);
        close(pipefd[0]);

        execvp(cmd->next->arguments[0], cmd->next->arguments);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);
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
    if (historyCount == HISTLEN) {
        free(history[0]);
        for (int i = 1; i < HISTLEN; i++) {
            history[i - 1] = history[i];
        }
        historyCount--;
    }
    history[historyCount++] = strdup(cmd);
}

// Print the history
void printHistory() {
    for (int i = 0; i < historyCount; i++) {
        printf("%d %s\n", i + 1, history[i]);
    }
}

// Get a command from history
char *getHistoryCommand(int index) {
    if (index < 1 || index > historyCount) {
        fprintf(stderr, "Invalid history index.\n");
        return NULL;
    }
    return history[index - 1];
}
