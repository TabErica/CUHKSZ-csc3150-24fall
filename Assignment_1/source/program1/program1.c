#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

// func declaration: returns the signal name from the signal number
const char* get_signal_name(int sig_num);


int main(int argc, char *argv[]){

	pid_t pid;
    int status;

	/* fork a child process */
	printf("Process start to fork\n");
    pid = fork();

    if (pid == -1){
        perror("error");
        exit(1);
    }

    else{
        if (pid == 0){
			// Child process logic
            char *arg[argc];
			for (int i = 0; i < argc - 1; i++) {
				arg[i] = argv[i + 1];
			};
			arg[argc - 1] = NULL;

			printf("I'm the Child Process, my pid = %d\n",
			       getpid()); 


			/* execute test program */
			printf("Child process start to execute test program:\n");
			execve(arg[0], arg, NULL);
			// If execve fails, print error and terminate child process
			perror("execve");
			exit(EXIT_FAILURE);
        }

        else{
			// Parent process logic
			printf("I'm the Parent Process, my pid = %d\n", getpid());

			/* wait for child process terminates */
            waitpid(pid, &status, WUNTRACED);
            printf("Parent process receives the SIGCHLD signal\n");

			/* check child process'  termination status */
			// If the child terminated normally
            if (WIFEXITED(status)){
				printf("Normal termination with EXIT STATUS = %d\n", WEXITSTATUS(status));
				}
			// If the child process was stopped
			else if (WIFSTOPPED(status)){
				printf("child process get SIGSTOP signal\n");
				}
			// If the child process was terminated by a signal
			else if (WIFSIGNALED(status)){
					int sig = WTERMSIG(status);
					printf("child process get %s signal\n", get_signal_name(sig));
					} 
			// Handle other cases
			else{
					printf("CHILD PROCESS CONTINUED\n");
					}

				exit(0);
        }
    }
    return 0;
		
}


/* 
	Get signal name from signal number
	Input: Signal number (sig_num)
	Output: Signal name string 
*/
const char* get_signal_name(int sig_num) {
    switch (sig_num) {
        case 1: return "SIGHUP";
        case 2: return "SIGINT";
        case 3: return "SIGQUIT";
        case 4: return "SIGILL";
        case 5: return "SIGTRAP";
        case 6: return "SIGABRT";
        case 7: return "SIGBUS";
        case 8: return "SIGFPE";
        case 9: return "SIGKILL";
        case 11: return "SIGSEGV";
        case 13: return "SIGPIPE";
        case 14: return "SIGALRM";
        case 15: return "SIGTERM";
        case 19: return "SIGSTOP";
        default: return "Unknown Signal";
    }
}
