#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    int pipefd[2]; // Pipe file descriptors
    pid_t child1, child2;

    // Step 1: Create the pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process> forking...)\n");

    // Step 2: Fork the first child
    if ((child1 = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child1 == 0) { // Child 1
        fprintf(stderr, "(child1> redirecting stdout to the write end of the pipe...)\n");
        close(STDOUT_FILENO); // Close standard output
        dup(pipefd[1]); // Duplicate write-end of pipe to stdout
        close(pipefd[1]); // Close the original write-end
        close(pipefd[0]); // Close the read-end of the pipe

        fprintf(stderr, "(child1> going to execute cmd: ls -l)\n");
        char *cmd[] = {"ls", "-l", NULL};
        execvp(cmd[0], cmd);

        // If execvp fails
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process> created process with id: %d)\n", child1);

    // Step 3: Parent process closes the write end of the pipe
    fprintf(stderr, "(parent_process> closing the write end of the pipe...)\n");
    close(pipefd[1]);

    fprintf(stderr, "(parent_process> forking...)\n");

    // Step 4: Fork the second child
    if ((child2 = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child2 == 0) { // Child 2
        fprintf(stderr, "(child2> redirecting stdin to the read end of the pipe...)\n");
        close(STDIN_FILENO); // Close standard input
        dup(pipefd[0]); // Duplicate read-end of pipe to stdin
        close(pipefd[0]); // Close the original read-end

        fprintf(stderr, "(child2> going to execute cmd: tail -n 2)\n");
        char *cmd[] = {"tail", "-n", "2", NULL};
        execvp(cmd[0], cmd);

        // If execvp fails
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process> created process with id: %d)\n", child2);

    // Step 5: Parent process closes the read end of the pipe
    fprintf(stderr, "(parent_process> closing the read end of the pipe...)\n");
    close(pipefd[0]);

    // Step 6: Parent waits for child processes to terminate
    fprintf(stderr, "(parent_process> waiting for child processes to terminate...)\n");
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    fprintf(stderr, "(parent_process> exiting...)\n");
    return 0;
}