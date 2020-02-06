#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>


pthread_mutex_t lock;

typedef struct arg_struct{
    int ** arg1;
    int * arg2;
} arg_struct;

void error(const char *msg) { fprintf(stderr, msg); exit(0); } // Error function used for reporting issues

//To clean up array for the mutex
void freeChildPids(int ** childPids){
  int i;
  free(*childPids);
  childPids = NULL;
}

// Checks to free the pids to grab more sockets
void checkForDonePids(int ** childPids, int * numPids){
  if(childPids == NULL || numPids == NULL){
    return;
  }
  int i, status, j;
  int * newChildPids = malloc(sizeof(int) * 5); // Allocate new mem for resized array

  j = 0;
  for( i = 0; i < *numPids; i++){
    if( waitpid((*childPids)[i], &status, WNOHANG) == 0 ){ // Checks status of processes
      newChildPids[j] = (*childPids)[i];
      j += 1;
    }
    else{
      //printf("background pid %d is done: exit value %d\n", (*childPids)[i], status);
      //fflush(stdout);
      kill((*childPids)[i], 0);
    }
  }

  //Free the pids, update current working pids
  pthread_mutex_lock(&lock);
  freeChildPids(childPids);
  *childPids = newChildPids;
  *numPids = j;
  pthread_mutex_unlock(&lock);
}

// Thread to watch the forks and close them when they are done to add
// more server connections as they come in.
void *socketWatcher(void * arguments) {
  struct arg_struct *args = arguments;
  int ** childPids = args->arg1;
  int * numPids = args->arg2;
  while(1){
    checkForDonePids(childPids, numPids);
    usleep(50000);
  }
}

// Creates the thread, adds the address for childPids
void createSocketWatcherThread(int ** childPids, int * numPids){
  pthread_t thread_id;
  // Need to pass two args into pthread
  struct arg_struct args;
    args.arg1 = childPids;
    args.arg2 = numPids;
  pthread_create(&thread_id, NULL, socketWatcher, (void *)&args);
}

// Mod instead of simple remainder for negative number modding
int mod(int a, int b) {
  int m = a % b;
  if (m < 0) {
    m = (b < 0) ? m - b : m + b;
  }
  return m;
}


//Message should be messageToDecode@Key
// Main function to handle the decoding of message
char * decodeMessage(char * rawMessage){
  int i, j, k, keyNum, messageNum;
  char * decodedString;
  char decodedLetter;

  // Get the key string
  char * keyStr = strstr(rawMessage, "@");
  int keyLength = strlen(keyStr) - 1;

  // Get the message string, assign to another string for encoding
  decodedString = malloc(sizeof(char) * (keyLength + 1));
  memset(decodedString, '\0', keyLength + 1);
  int messageLen = (keyStr - rawMessage) / sizeof(char);
  strncpy(decodedString, rawMessage, messageLen);

  j = 0;
  // Iterates through the key, encrypting as it goes. If key is bigger than
  // the message, the loop starts at the beginning of the message again and
  // continues to encrypt.
  for (i = 0; i < keyLength; i++){
    if(decodedString[j] == '\0'){
      j = 0;
    }

    // 0=space, A=1, B=2 ...
    keyNum = ((int)keyStr[i+1] - 64);
    messageNum = ((int)decodedString[j] - 64);

    //If space, convert to 0
    keyNum = keyNum < 0 ? 0 : keyNum;
    messageNum = messageNum < 0 ? 0 : messageNum;

    keyNum = messageNum - keyNum;
    keyNum = mod(keyNum, 27);
    decodedLetter = keyNum == 0 ? ' ' : (char)(keyNum + 64);
    // printf("Cypher: %c   Key: %c\n", rawMessage[i], keyStr[i+1]);
    // printf("decodedLetter Letter: %c with keyNum:%d\n", decodedLetter, keyNum);

    // Update the string
    decodedString[j] = decodedLetter;
    j++;
  }
  return decodedString;
}

// Runs on the forked child process
int runChildProcess(int * establishedConnectionFD){
  char message[140000];
  char decodedMessage[70001];

  char buffer[2048];
  char buff2[20];
  char * temp;
  int charsRead, totalCharsRead, nextMessageLength;
  int i;
  // Get the message from the client and verify data

  //Preform init handshake, verify process and number of chars to expect
  //SHOULD EXPECT "otp_dec@xxxxx\0"
  memset(buffer, '\0', 70000);
  memset(buff2, '\0', 20);
  charsRead = recv(*establishedConnectionFD, buffer, 255, 0); // Check length
  if (charsRead < 0) error("ERROR reading from socket");
  if ( strncmp(buffer, "otp_dec", 7) == 0){
    strcpy(buff2, strstr(buffer, "@"));
    buff2[strcspn(buff2, "@")] = '0'; // Convert @ to 0 for atoi
    nextMessageLength = atoi(buff2);
    //printf("Sending success\n");
    send(*establishedConnectionFD, "HS", 2, 0); // Handshake successful
  }
  else{
    charsRead = send(*establishedConnectionFD, "HF", 2, 0); // Handshake failed
    return 0; // Fail
  }

  // Now that handshake is complete, we need to get the sause
  memset(message, '\0', 140000);
  memset(buffer, '\0', sizeof(buffer));

  charsRead = 0;
  //printf("%d", charsRead);
  // Verify the number of bytes recieved is as expected and copy into buffer
  while (recv(*establishedConnectionFD, message, sizeof(message), MSG_PEEK) < nextMessageLength){
    usleep(500);
  }
  if ( (charsRead += recv(*establishedConnectionFD, message, sizeof(message), 0)) == 0 ){
    error("Failed to transfer bytes...");
  }


  memset(decodedMessage, '\0', sizeof(decodedMessage));
  strcpy(decodedMessage, decodeMessage(message));

  charsRead = send(*establishedConnectionFD, decodedMessage, strlen(decodedMessage), 0); // Send success back
  if (charsRead < 0) error("ERROR writing to socket");

  _exit(0);
}

// Handles the running of the server. Looks for free pid slots, holds for a
// socket connection, creates new fork to handle that socket connections
// Cleans memory as it goes
void runServer(int listenSocketFD){
  int establishedConnectionFD;
  struct sockaddr_in clientAddress;
  socklen_t sizeOfClientInfo;
  int * childPids = malloc(sizeof(int) * 5);
  int numPids = 0;

  // Thread to watch for done forks
  createSocketWatcherThread(&childPids, &numPids);


  while(1){
    while(numPids <= 5){
      sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
    	establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
    	if (establishedConnectionFD < 0){
        error("ERROR on accept");
      }

      pid_t pid = fork();
      switch(pid){
        case -1:
          error("HULL BREACH");
          exit(1);
          break;
        case 0: ;
         // Child process
         runChildProcess(&establishedConnectionFD);
          _exit(0);
          break;
        default: // Parent
          pthread_mutex_lock(&lock);
          childPids[numPids] = pid;
          numPids += 1;
          pthread_mutex_unlock(&lock);
      }
    }
  }
  	close(establishedConnectionFD); // Close the existing socket which is connected to the client
  	close(listenSocketFD); // Close the listening socket
}



int main(int argc, char *argv[])
{
  int listenSocketFD, establishedConnectionFD, portNumber;


	struct sockaddr_in serverAddress;
	if (argc < 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args

	// Set up the address struct for this process (the server)
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[1]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process

	// Set up the socket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (listenSocketFD < 0){
    error("ERROR opening socket");
    return 1;
  }


	// Enable the socket to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		error("ERROR on binding");
	listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections


  if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }

  runServer(listenSocketFD);
	return 0;
}
