/**************************************************************************************
 **  Name: Maliha Syed
 **  Class: CS344
 **  Title: Program 3
 **  Date: 2/13/20
 **  Description: Implementation of a small shell.  This will allow for the redirection
 **  of standard input and standard output and will support both foreground and
 **  background processes (controllable by the command line and by receiving signals).
 **  The shell will support three built in commands: exit, cd, and status. It will also
 **  support comments beginning with the # character.
 ***************************************************************************************/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>


#define MAX_ARGS 513

int numForks = 0;

int parentPID; // pid of parent shell process
char *lineEntered = NULL; // points to a buffer allocated by getline() that holds the entered string + \n + \0
char *commands[MAX_ARGS]; // array of pointers to each of the commands/arg in the user input
int pidArray[100]; // holds the pids of bg processes
int numCmds = 0; // holds the number of commands in the commands array
int numBgs = 0; // holds the number of bg processes in the pidArray
int done = 0; // flag for running the program
int bg = 0; // flag for if the child process is a background process
FILE *inputFile; // pointer to input file for redirection
FILE *outputFile; // pointer to output file for redirection
int isCtrlZ = 0; // flag for ctrl+z signal
int fgStatus = -5; // exit status of last fg process


/**************************************************************************************
 ** catchSIGTSTP - handler for SIGTSTP signal, prints message based on isCtrlZ flag
 **************************************************************************************/
void catchSIGTSTP(int signo)
{
  if (isCtrlZ == 0) // if ctrlz has not been pressed
  {
    isCtrlZ = 1; // switch flag
    char* message = "\nEntering foreground-only mode (& is now ignored)\n: ";
    write(1, message, 52);
    fflush(stdout);
  }
  else
  {
    isCtrlZ = 0; // switch flag
    char* message = "\nExiting foreground-only mode\n: ";
    write (1, message, 32);
    fflush(stdout);
  }
}


/**************************************************************************************
 ** cleanUpBgs - function that iterates through the bg pid array and prints out either
 ** the exit value or termination signal of any completed bg child processes
 **************************************************************************************/
void cleanUpBgs() {
  int exitStatus;
  int i;
  for (i = 0; i < numBgs; i++) {
    if(waitpid(pidArray[i], &exitStatus, WNOHANG) > 0) { // if process complete
      if(WIFEXITED(exitStatus)) { // if child exited
        printf("background pid %d is done: exit value %d\n", pidArray[i], WEXITSTATUS(exitStatus));
        fflush(stdout);
      }
      if(WIFSIGNALED(exitStatus)) { // if child was terminated by a signal
        printf("background pid %d is done: terminated by signal %d\n", pidArray[i], WTERMSIG(exitStatus));
        fflush(stdout);
      }
    }
  }
}


/**************************************************************************************
 ** getUserInput - uses getline to get user input, expands any occurrence of '$$' into
 ** the pid of the shell process, and tokenizes the parsed line into an array
 **************************************************************************************/
void getUserInput()
{
  int numCharsEntered = -5; // How many chars were entered
  size_t bufferSize = 0; // Holds how large the allocated buffer is
  char *token;
  
  memset(commands, '\0', sizeof(commands)); //reset
  numCmds = 0; //reset

    while(1)
    {
      clearerr(stdin);
      printf("\r: ");
      fflush(stdout);
      // Get a line from the user
      numCharsEntered = getline(&lineEntered, &bufferSize, stdin);
      if (numCharsEntered == -1 || numCharsEntered == 1 || lineEntered[0] == '#')
        clearerr(stdin);
      else
        break; // Exit the loop - we've got input
    }
  
    lineEntered[strcspn(lineEntered, "\n")] = '\0'; // replace newline with null terminator

    char pidString[20];
    memset(pidString, '\0', sizeof(pidString));
    sprintf(pidString, "%d", parentPID);
    char buffer[5120]; // holds the new line after replacement with pid
    long lenPid = strlen(pidString);
    int len$$ = strlen("$$");
  
    int c = 0;
    while (*lineEntered)
    {
      if (strstr(lineEntered, "$$") == lineEntered) // find all occurrences of "$$"
      {
        strcpy(&buffer[c], pidString); //  append pid to buffer at index c
        c += lenPid; // increment c by length of pid
        lineEntered += len$$; // increment line by length of "$$"
      }
      else
      buffer[c++] = *lineEntered++; // add char to buffer if not "$$"
    }
    buffer[c] = '\0';
  
    token = strtok(buffer, " "); // tokenize on space delimiter
  
    // tokenize line and add to commands array
    int i = 0;
    while(token != NULL && i < MAX_ARGS) {
      commands[i] = token;
      numCmds++;
      token = strtok(NULL, " ");
      i++;
    }
  
}


/**************************************************************************************
 ** checkBuiltIns - checks if the command is 'cd', 'status', or exit, and defines
 ** custom behaviors for each one
 **************************************************************************************/
void checkBuiltIns()
{
  // check if command is exit
  if ( strcmp(commands[0], "exit") == 0 ) {
    // kill all processes and quit program
    done = 1;
    exit(0);
  }
  
  // check if command is status
  if ( strcmp(commands[0], "status") == 0 ) {
    // prints out the exit status or the terminating signal of the last foreground process
    if (fgStatus == -5) {
      printf("exit value 0\n");
      fflush(stdout);
    }
    else if (fgStatus < 2) {
      printf("exit value %d\n", fgStatus);
      fflush(stdout);
    }
    else {
      printf("terminated by signal %d\n", fgStatus);
    }
    fflush(stdout);
  }
  
  // check if command is cd
  if ( strcmp(commands[0], "cd") == 0 ) {
    if (numCmds == 1) { // if there is no argument, changes to dir in HOME env
      chdir(getenv("HOME"));
    }
    else if (numCmds == 2) { // enters specified directory if valid
      char dir[300];
      getcwd(dir, sizeof(dir));
      char *path = commands[1];
      strcat(dir, "/");
      strcat(dir, path);
      
      if (chdir(dir) != 0) { // error if directory doesn't exist
        perror("chdir failed");
      }
    }
  }
}


/**************************************************************************************
 ** isBackground - function that determines if command specifies a background process.
 ** sets the bg flag if last argument is & and the isCtrlZ flag is 0 (false)
 **************************************************************************************/
void isBackground()
{
  // check if last token in commands is & and set flag
  if (*commands[numCmds-1] == '&') {
    commands[numCmds-1] = NULL; // remove bg ampersand
    numCmds--;
    
    if (isCtrlZ == 0) { // if not in foreground only mode, then set bg
      bg = 1;
    }
    else {
      bg = 0;
    }
  }
  else {
    bg = 0;
  }
}


/**************************************************************************************
 ** forkChild - forks a child process, checks commands for file redirection, child
 ** process execs and parent calls waitpid if process is foreground
 **************************************************************************************/
void forkChild(struct sigaction *SIGINT_action, struct sigaction *ignore_action, struct sigaction *SIGTSTP_action) {
  
  pid_t spawnpid = -5;
  spawnpid = fork(); // fork a child process with a new pid
  
  if (spawnpid == -1) { // if fork is unsuccessful
    perror("error in fork\n");
    exit(1);
  }
  
  else if (spawnpid == 0) { // fork successful, in child process
    
    sigaction(SIGTSTP, ignore_action, NULL); // ignore ctrlz while process is running
    
    // respond to SIGINT if fg process
    if (bg == 0) {
      sigaction(SIGINT, SIGINT_action, NULL); // respond to ctrlc while fg process is running
    }
    
    int j;
    // check for input redirection
    for (j = 0; j < numCmds; j++) {
      if (*commands[j] == '<') {
        commands[j] = NULL; // set redirect to null
        close(STDIN_FILENO);
        if(bg == 1 && commands[j+1] == NULL) { // if no file name is get from dev/null
          inputFile = fopen("/dev/null", "r");
        }
        else { // redirect input from inputFile instead of stdin
          inputFile = fopen(commands[j+1], "r");
          if (inputFile == NULL) {
            perror("Failed");
            exit(1);
          }
        }
        
      }
      
      // check for output redirection
      else if (*commands[j] == '>') {
        commands[j] = NULL;
        close(STDOUT_FILENO);
        if(bg == 1 && commands[j+1] == NULL) { // if no file name is given send to dev/null
          fflush(stdout);
          outputFile = fopen("/dev/null", "w");
        }
        else { // redirect output to outputFile instead of to stdout
          outputFile = fopen(commands[j+1], "w");
          if (!outputFile) {
            perror("Failed");
            exit(1);
          }
        }

      }
      
    }

    // call exec to run commands
    if (execvp(*commands, commands) < 0){
      perror("Exec failure");
      exit(1);
    }
    
    // only print if argument is bad and exec cannot run
    printf("%s: no such file or directory\n", commands[0]);
    
    
  }
  
  // parent process continues
  else {
    sigprocmask(SIG_BLOCK, &SIGTSTP_action->sa_mask, NULL);
    if (bg == 1) { // if child is bg, add pid to array and print pid
      pidArray[numBgs] = spawnpid;
      numBgs++;
      printf("background pid is %d\n", spawnpid);
      fflush(stdout);
    }
    else { // if child is fg, wait for child to finish and print if terminated
      
      
      int childExitStatus;
      
      waitpid(spawnpid, &childExitStatus, 0); // wait for fg process to complete
      
      // Exited normally
      if (WIFEXITED(childExitStatus) != 0)
      {
        fgStatus = WEXITSTATUS(childExitStatus);
      }
      // Terminated by signal
      else
      {
        fgStatus = WTERMSIG(childExitStatus);
        printf("terminated by signal %d\n", fgStatus);
        fflush(stdout);
      }
    }
  }
}


/**************************************************************************************
 ** main - defines sigaction and signal handlers, calls the functions to execute user
 ** inputted commands in a loop until exit is entered, then cleans up and exits
 **************************************************************************************/
int main(int argc, const char * argv[]) {
  
  // declare 3 sigaction structs
  struct sigaction SIGINT_action = {0};
  struct sigaction SIGTSTP_action = {0};
  struct sigaction ignore_action = {0};
  
  // defined signal handlers
  SIGINT_action.sa_handler = SIG_DFL;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;

  SIGTSTP_action.sa_handler = catchSIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = SA_RESTART;

  ignore_action.sa_handler = SIG_IGN;

  sigaction(SIGINT, &ignore_action, NULL); // ignore SIGINT
  sigaction(SIGTSTP, &SIGTSTP_action, NULL); // call function catchSIGTSTP for SIGTSTP

  
  parentPID = getpid(); // saves parent shell pid
  

  memset(commands, '\0', sizeof(commands));
  memset(pidArray, '\0', sizeof(pidArray));
  
  // program loop runs until 'exit' is inputted
  while (done == 0) {
    sigprocmask(SIG_UNBLOCK, &SIGTSTP_action.sa_mask, NULL);
    cleanUpBgs();
    getUserInput();
    if ( strcmp(commands[0], "exit") == 0 || strcmp(commands[0], "cd") == 0 || strcmp(commands[0], "status") == 0) {
      checkBuiltIns();
    }
    else {
      isBackground();
      forkChild(&SIGINT_action, &ignore_action, &SIGTSTP_action);
    }
    
  }
  
  // kill all background processes
  int k;
  for (k = 0; k < numBgs; k++) {
    kill(pidArray[k], SIGKILL);
    k++;
  }
  numBgs = 0;
  
  
  //free all heap memory
  free(inputFile);
  inputFile = NULL;
  free(outputFile);
  outputFile = NULL;
  free(lineEntered);
  lineEntered = NULL;
  
  return 0;
}

// CITATIONS:
// Class notes section 3.3 for using getline
// Class lecture code for signals
// https://www.tutorialspoint.com/unix_system_calls/waitpid.htm
