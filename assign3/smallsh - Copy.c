#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>




struct sigaction SIGINT_action;
struct sigaction SIGTSTP_action;
int ENTER_IGNORE_BACKGROUND = 0;
int PRINT_IGNORE_FLAG = 0;
int STATUS;




void print(char output[]){
  printf(output);
  fflush(stdout);
}

void catchSIGINT(int signo) {
  STATUS = 2;
  print("terminated by signal 2\n");
}

void catchSIGTSTP(int signo) {
  ENTER_IGNORE_BACKGROUND = !ENTER_IGNORE_BACKGROUND;
  PRINT_IGNORE_FLAG = 1;
}


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


int runBuiltInCommands(char ** commands, int numCommands) {
  char cwd[256];
  if(strcmp(commands[0], "exit") == 0){
    
    exit(0);
  }
  else if(strcmp(commands[0], "cd") == 0){
    if(numCommands == 1) {
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
    if(STATUS == SIGINT)
      strcpy(output, "terminated by signal 2\n");
    else{
      exitVal = STATUS == 0 ? 0 : 1;
      strcpy(output, "exit value ");
      sprintf(output, "%s%d", output, exitVal);
    }
    print(output);
    return 1;
  }
  return 0;
}


void resetCommands(char ** commands, int * numCommands){
  int i;
  for(i = 0; i < *numCommands; i++){
    free(commands[i]);
  }
  *numCommands = 0;
}


void deleteCommand(char *** commands, int * numCommands, int indexToDelete){
  char ** newCommands = malloc(sizeof(char*) * (*numCommands - 2));
  int i;
  int j = 0; // newCounter
  for(i = 0; i < *numCommands - 1; i++){  // Minus 1 to ignore the null
    if(i != indexToDelete && i != indexToDelete + 1){
      newCommands[j] = malloc(sizeof(char) * (strlen((*commands)[i] + 1)));
      strcpy(newCommands[j], (*commands)[i]);
      j += 1;
    }
  }
  i = *numCommands;
  resetCommands(*commands, numCommands);
  *commands = newCommands;
  *numCommands = i - 2;
}

void badFile(char * outputText){
  STATUS = 1;
  print(outputText);
  exit(1);
}



void handleRedirections(char *** commands, int * numCommands){
  int i;
  int fd;
  for(i = 0; i < *numCommands - 1; i++){
    if(strcmp((*commands)[i],"<") == 0){  // Redirect input
      if(freopen((*commands)[i + 1], "r", stdin) == NULL){
        char outputText[80];
        sprintf(outputText, "%s%s%s", "cannot open ", (*commands)[0], " for input");
        badFile(outputText);
      }
      deleteCommand(commands, numCommands, i);
      i = 0;
    }
    else if(strcmp((*commands)[i],">") == 0){ // Redirect output
      if(freopen((*commands)[i + 1], "w+", stdout) == NULL){
        char outputText[80];
        sprintf(outputText, "%s%s%s", "cannot open ", (*commands)[0], " for input");
        badFile(outputText);
      }
      deleteCommand(commands, numCommands, i);
      i = 0;
    }
  }
}


void checkForDonePids(int ** childPids, int * numChildPids){
  int i, status, j;
  int * newChildPids = malloc(sizeof(int) * 512);

  j = 0;
  for( i = 0; i < *numChildPids; i++){
    if( waitpid((*childPids)[i], &status, WNOHANG) == 0 ){
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




int runBashCommands(char ** commands, int * numCommands, int ** childPids, int * numChildPids){
  int status;

  if(ENTER_IGNORE_BACKGROUND && strcmp(commands[*numCommands - 1], "&") == 0){
    commands[*numCommands - 1] = NULL;
    *numCommands = *numCommands - 1;
  }

  pid_t pid = fork();
  switch(pid){
    case -1:
      perror("HULL BREACH");
      exit(1);
      break;
    case 0:     // Child process
      //signal(SIGTSTP, SIG_IGN);
      if(strstr(commands[0], "#") != NULL)  // Checks for comment functionality
        return 1;

      if(strcmp(commands[*numCommands - 1], "&") == 0){  // Run in the background
        int devNull = open("/dev/null", O_RDWR);
        dup2(devNull, 0);
        dup2(devNull, 1);
        commands[*numCommands - 1] = NULL; // replaces & with NULL for execvp
      }
      else{
        // Adds NULL for execvp functionality
        commands[*numCommands] = malloc(sizeof(char));
        commands[*numCommands] = NULL;
        *numCommands += 1;
      }
      handleRedirections(&commands, numCommands);
      if(execvp(commands[0], commands) == -1){
        if(strcmp(commands[0], "") != 0){
          char outputText[128];
          sprintf(outputText, "%s%s", commands[0], ": no such file or directory\n");
          badFile(outputText);
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
        waitpid(pid, &status, 0);
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


void checkForPidExpansion(char ** commands, int numCommands){
  int i;
  char ppid[10];
  char oldCommand[512];
  char * strLoc;
  sprintf(ppid, "%d", getpid());

  for (i = 0; i < numCommands; i++){
    if( (strLoc = strstr( commands[i], "$$")) != NULL ){
      strcpy(strLoc,strLoc + 2);
      strcpy(oldCommand, commands[i]);

      commands[i] = malloc(sizeof(char) * (strlen(ppid) + strlen(commands[i])));

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

  getenv("PATH");


  //init signal

  SIGINT_action.sa_flags = 0;
  SIGTSTP_action.sa_flags = 0;

  sigfillset(&SIGINT_action.sa_mask);
  sigfillset(&SIGTSTP_action.sa_mask);


  while(1){
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

    getUserCommands(commands, &numCommands);
    checkForPidExpansion(commands, numCommands);
    if(numCommands > 0){
      if(runBuiltInCommands(commands, numCommands)){
        resetCommands(commands, &numCommands);
      }
      else{
        runBashCommands(commands, &numCommands, &childPids, &numChildPids);
        resetCommands(commands, &numCommands);
      }
    }
  }


}
