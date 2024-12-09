#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define EXIT_FAILURE -1
#define EXIT_SUCCESS 0

int main() {
    int pipefd[2];  // File descriptors for the pipe
    pid_t child1, child2;  // Process IDs for child processes

    // Create the pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Debug message
    fprintf(stderr, "(parent_process>forking...)\n");

    // Fork the first child
    child1 = fork();
    if (child1 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }


    if (child1 == 0) {
        // First child process (child1)
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");

        // Redirect stdout to the write end of the pipe
        close(pipefd[0]);  // Close the unused read end
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);  // Close the original file descriptor

        // Execute "ls -l"
        fprintf(stderr, "(child1>going to execute cmd: ls -l)\n");
        execlp("ls", "ls", "-l", NULL);

        // If execlp fails
        perror("execlp");
        exit(EXIT_FAILURE);
    }

    // Parent process
    fprintf(stderr, "(parent_process>created process with id: %d)\n", child1);
    fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
    close(pipefd[1]);  // Close the write end of the pipe  COMENNT 4

    // Fork the second child
    fprintf(stderr, "(parent_process>forking...)\n");
    child2 = fork();
    if (child2 == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child2 == 0) {
        // Second child process (child2)
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");

        // Redirect stdin to the read end of the pipe
        close(pipefd[1]);  // Close the unused write end
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);  // Close the original file descriptor

        // Execute "tail -n 2"
        fprintf(stderr, "(child2>going to execute cmd: tail -n 2)\n");
        execlp("tail", "tail", "-n", "2", NULL);

        // If execlp fails
        perror("execlp");
        exit(EXIT_FAILURE);
    }

    // Parent process
    fprintf(stderr, "(parent_process>created process with id: %d)\n", child2);
    fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
    close(pipefd[0]);  // Close the read end of the pipe COMMENT 7

    // Wait for the child processes to terminate
    fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    fprintf(stderr, "(parent_process>exiting...)\n");
    return EXIT_SUCCESS;
}
