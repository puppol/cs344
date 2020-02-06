#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

// SET UP GLOBALS FOR SIGNAL HANDLING

struct sigaction SIGINT_action;
struct sigaction SIGTSTP_action;
int ENTER_IGNORE_BACKGROUND = 0;
int PRINT_IGNORE_FLAG = 0;
int STATUS;


// Custom print to make sure flush is called each time
void print(char output[]){
  printf(output);
  fflush(stdout);
}


// Custom SIGINT handling function
void catchSIGINT(int signo) {
  STATUS = 2;
  print("terminated by signal 2\n");
}


// Custom SIGTSTP handling function
void catchSIGTSTP(int signo) {
  ENTER_IGNORE_BACKGROUND = !ENTER_IGNORE_BACKGROUND;
  PRINT_IGNORE_FLAG = 1;
}

// Smart tokenizer, also removes new line characters
void splitUserInput(char * userInput, char ** commands, int * numCommands){
  int tempLength;
  // Split into commands
  char * pch;
  pch = strtok(userInput," ");
  while(pch != NULL){
    // Save mem
    commands[*numCommands] = malloc(sizeof(char) * (strlen(pch) + 1));

    strcpy(commands[*numCommands], pch);
    // Change newline to null
    commands[*numCommands][strcspn(commands[*numCommands],"\n")] = 0;
    pch = strtok (NULL, " ");
    *numCommands += 1;
  }
}

void getUserCommands(char ** commands, int * numCommands){
  //Make new array
  int maxInputSize = 2048;
  char ** userCommands;
  char userInput[maxInputSize + 1];
  memset(userInput, '\0', maxInputSize + 1);

  // Prompt user command using ':'
  print(": ");

  // Read input
  fgets(userInput, maxInputSize, stdin);

  // Cut input into commands
  // Make dynarray from those commands
  splitUserInput(userInput, commands, numCommands);
}

// Used to run the 3 built in commands as required.
int runBuiltInCommands(char ** commands, int numCommands) {
  char cwd[256];
  // Exit
  if(strcmp(commands[0], "exit") == 0){
    exit(0);
  }
  else if(strcmp(commands[0], "cd") == 0){
    if(numCommands == 1) {
      // Switch to home
      return chdir(getenv("HOME")) == 0 ? 1 : 0;
    }
    else{
      return chdir(commands[1]) == 0 ? 1 : 0;
    }
    return 0;
  }
  else if(strcmp(commands[0], "status") == 0){
    int exitVal;
    char output[256];
    // Grabs the global status flag of the last command ran
    if(STATUS == SIGINT)
      strcpy(output, "terminated by signal 2\n");
    else{
      exitVal = STATUS == 0 ? 0 : 1;
      // Sets up print value stifled
      strcpy(output, "exit value ");
      sprintf(output, "%s%d", output, exitVal);
    }
    print(output);
    return 1;
  }
  return 0;
}

// Resets the commands array for the next user command
void resetCommands(char ** commands, int * numCommands){
  int i;
  for(i = 0; i < *numCommands; i++){
    free(commands[i]);
  }
  *numCommands = 0;
}

// Deletes the < and > characters and their following file
void deleteCommand(char *** commands, int * numCommands, int indexToDelete){
  char ** newCommands = malloc(sizeof(char*) * (*numCommands - 2));
  int i;
  int j = 0; // newCounter
  for(i = 0; i < *numCommands - 1; i++){  // Minus 1 to ignore the null
    if(i != indexToDelete && i != indexToDelete + 1){
      newCommands[j] = malloc(sizeof(char) * (strlen((*commands)[i] + 1))); // Allocate new mem
      strcpy(newCommands[j], (*commands)[i]);
      j += 1;
    }
  }
  i = *numCommands;
  // Reset pointers to new memory
  resetCommands(*commands, numCommands);
  *commands = newCommands;
  *numCommands = i - 2;
}


// Custom output for when there is a bad file
void badFile(char * outputText){
  STATUS = 1;
  print(outputText);
  exit(1);
}

// Overarching function to handle redirections
void handleRedirections(char *** commands, int * numCommands){
  int i;
  int fd;
  for(i = 0; i < *numCommands - 1; i++){
    if(strcmp((*commands)[i],"<") == 0){  // Redirect input
      if(freopen((*commands)[i + 1], "r", stdin) == NULL){ // Pushes new file to stdin
        char outputText[80];
        sprintf(outputText, "%s%s%s", "cannot open ", (*commands)[2], " for input");
        badFile(outputText);
      }
      // Cleans up, gets ready for next iteration in the for loop
      deleteCommand(commands, numCommands, i);
      i = 0;
    }
    else if(strcmp((*commands)[i],">") == 0){ // Redirect output
      if(freopen((*commands)[i + 1], "w+", stdout) == NULL){ // Pushes new file to stdout
        char outputText[80];
        sprintf(outputText, "%s%s%s", "cannot open ", (*commands)[2], " for input");
        badFile(outputText);
      }
      deleteCommand(commands, numCommands, i);
      i = 0;
    }
  }
}

// Checks through the list of background pids to see if they are done, prints out to screen.
void checkForDonePids(int ** childPids, int * numChildPids){
  int i, status, j;
  int * newChildPids = malloc(sizeof(int) * 512); // Allocate new mem for resized array

  j = 0;
  for( i = 0; i < *numChildPids; i++){
    if( waitpid((*childPids)[i], &status, WNOHANG) == 0 ){ // Checks status of processes
      newChildPids[j] = (*childPids)[i];
      j += 1;
    }
    else{
      printf("background pid %d is done: exit value %d\n", (*childPids)[i], status);
      fflush(stdout);
    }
  }

  *childPids = newChildPids;
  *numChildPids = j;
}



// Overarching function for spawning, controlling, and waiting for child processes
int runBashCommands(char ** commands, int * numCommands, int ** childPids, int * numChildPids){
  int status;

  // Makes sure that you can run a background command if asked to do so
  if(ENTER_IGNORE_BACKGROUND && strcmp(commands[*numCommands - 1], "&") == 0){
    commands[*numCommands - 1] = NULL;
    *numCommands = *numCommands - 1;
  }

  // SPAWN CHILD
  pid_t pid = fork();
  switch(pid){
    case -1:
      perror("HULL BREACH");
      exit(1);
      break;
    case 0:     // Child process
      //signal(SIGTSTP, SIG_IGN);
      if(strstr(commands[0], "#") != NULL)  // Checks for comment functionality
        exit(0);


      if(strcmp(commands[*numCommands - 1], "&") == 0){  // Run in the background
        int devNull = open("/dev/null", O_RDWR);
        dup2(devNull, 0);
        dup2(devNull, 1);
        commands[*numCommands - 1] = NULL; // replaces & with NULL for execvp
        SIGINT_action.sa_handler = SIG_IGN; // Ignores CTRL-Z as a bsackground process
      }
      else{
        SIGINT_action.sa_handler = SIG_DFL;
        // Adds NULL for execvp functionality
        commands[*numCommands] = malloc(sizeof(char));
        commands[*numCommands] = NULL;
        *numCommands += 1;
      }

      // Renews the handlers and functions
      SIGTSTP_action.sa_handler = SIG_IGN;
      sigaction(SIGINT, &SIGINT_action, NULL);
      sigaction(SIGTSTP, &SIGTSTP_action, NULL);

      // Makes sure redirections are set up
      handleRedirections(&commands, numCommands);
      if(execvp(commands[0], commands) == -1){
        if(strcmp(commands[0], "") != 0){
          char outputText[128];
          sprintf(outputText, "%s%s", commands[0], ": no such file or directory\n");
          badFile(outputText);
          exit(1);
        }
        else{exit(0);}
      }

      // Exit to prevent potential fork bombs
      exit(errno);
      break;
    default:
      if(strcmp(commands[*numCommands - 1], "&") != 0){  // Run in the foreground
        SIGINT_action.sa_handler = catchSIGINT;
        sigaction(SIGINT, &SIGINT_action, NULL);
        while(waitpid(pid, &status, 0) < 0){} // To wait for last command in foreground
        STATUS = status;
      }
      else{
        printf("background pid is %d\n", pid);
        fflush(stdout);
        (*childPids)[*numChildPids] = pid;
        *numChildPids += 1;
      }
      break;
  }
  return 1;
}

// Iterates through commands and replaces $$ with pid
void checkForPidExpansion(char ** commands, int numCommands){
  int i;
  char ppid[10];
  char oldCommand[512];
  char * strLoc;
  sprintf(ppid, "%d", getpid());

  for (i = 0; i < numCommands; i++){
    if( (strLoc = strstr( commands[i], "$$")) != NULL ){ // Grabs location
      strcpy(strLoc,strLoc + 2); // removes
      strcpy(oldCommand, commands[i]);

      commands[i] = malloc(sizeof(char) * (strlen(ppid) + strlen(commands[i])));
      // Appends pid into string
      strcpy(commands[i], oldCommand);
      strcat( commands[i], ppid);
    }
  }
}



int main(void){
  char ** commands;
  commands = malloc(sizeof(char * ) * 512);
  int numCommands = 0;
  int * childPids = malloc(sizeof(int) * 512);
  int numChildPids = 0;


  // Allows for PATH variables
  getenv("PATH");


  //init the signal handling
  SIGINT_action.sa_flags = 0;
  SIGTSTP_action.sa_flags = 0;

  sigfillset(&SIGINT_action.sa_mask);
  sigfillset(&SIGTSTP_action.sa_mask);


  while(1){ // Shell loop
    SIGINT_action.sa_handler = SIG_IGN;
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    if(PRINT_IGNORE_FLAG){
      PRINT_IGNORE_FLAG = 0;
      if(ENTER_IGNORE_BACKGROUND){
        print("\nEntering foreground-only mode (& is now ignored)\n");
      }
      else{
        print("\nExiting foreground-only mode\n");
      }
    }

    //check for pids
    checkForDonePids(&childPids, &numChildPids);
    // Gets the command line
    getUserCommands(commands, &numCommands);
    // Replaces $$ with pid
    checkForPidExpansion(commands, numCommands);
    if(numCommands > 0){
      // First check for built in
      if(runBuiltInCommands(commands, numCommands)){
        resetCommands(commands, &numCommands);
      }
      else{ // Else run child processes
        runBashCommands(commands, &numCommands, &childPids, &numChildPids);
        resetCommands(commands, &numCommands);
      }
    }
  }

  resetCommands(commands, &numCommands);
  return 0;
}
