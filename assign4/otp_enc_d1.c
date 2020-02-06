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

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues


void freeChildPids(int ** childPids){
  int i;
  free(*childPids);
  childPids = NULL;
}


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
  pthread_mutex_lock(&lock);
  freeChildPids(childPids);
  *childPids = newChildPids;
  *numPids = j;
  pthread_mutex_unlock(&lock);
}


void *socketWatcher(void * arguments) {
  struct arg_struct *args = arguments;
  int ** childPids = args->arg1;
  int * numPids = args->arg2;
  while(1){
    checkForDonePids(childPids, numPids);
    usleep(50000);
  }
}

void createSocketWatcherThread(int ** childPids, int * numPids){
  pthread_t thread_id;
  struct arg_struct args;
    args.arg1 = childPids;
    args.arg2 = numPids;
  pthread_create(&thread_id, NULL, socketWatcher, (void *)&args);
}

//Message should be key@messageToEncode
char * encodeMessage(char * rawMessage, int keyLength){
  int i, j, temp;
  char encodedLetter;
  char * encodedString;
  char * originalMessage = strstr(rawMessage, "@");
  encodedString = malloc(sizeof(char) * (keyLength + 1));
  memset(encodedString, '\0', keyLength + 1);

  j = 0;
  for (i = 0; i < keyLength; i++){
    if ( (rawMessage[i] != '\0')  && (originalMessage[i+1] != '\0') ) {
        temp = -1;
        if (rawMessage[i] == ' ') {
            temp = 64;
        }
      else{
        temp = (int)rawMessage[i] + (int)originalMessage[i+1];
        temp = temp % 26;
        temp = temp + 65;
        }
        if(temp != -1){
            encodedString[j] = (char)temp;
            j += 1;
        }
      }
  }
  return encodedString;
}



int runChildProcess(int * establishedConnectionFD){
  char message[140000];
  char buffer[1028];
  char buff2[20];
  char * temp;
  int charsRead, totalCharsRead, nextMessageLength;
  char * encodedMessage;
  int i;
  // Get the message from the client and verify data

  //Preform init handshake, verify process and number of chars to expect
  //SHOULD EXPECT "otp_enc@xxxxx\0"
  memset(buffer, '\0', 70000);
  memset(buff2, '\0', 20);
  charsRead = recv(*establishedConnectionFD, buffer, 255, 0); // Check length
  if (charsRead < 0) perror("ERROR reading from socket");
  if ( strncmp(buffer, "otp_enc", 7) == 0){
    strcpy(buff2, strstr(buffer, "@"));
    buff2[strcspn(buff2, "@")] = '0'; // Convert @ to 0 for atoi
    nextMessageLength = atoi(buff2);
    printf("Sending success\n");
    send(*establishedConnectionFD, "HS", 2, 0); // Handshake successful
  }
  else{
    charsRead = send(*establishedConnectionFD, "HF", 2, 0); // Handshake failed
    return 0; // Fail
  }


  // Now that handshake is complete, we need to get the sause
  memset(message, '\0', 70000);
  memset(buffer, '\0', 1028);

  charsRead = 0;
  printf("%d", charsRead);
  if ( (charsRead = recv(*establishedConnectionFD, buffer, 1028, 0)) > 0 ){
      strcat(message, buffer);
      memset(buffer, '\0', 1028);
  }


  charsRead = send(*establishedConnectionFD, encodeMessage(message, nextMessageLength / 2), (nextMessageLength / 2), 0); // Send success back
  if (charsRead < 0) perror("ERROR writing to socket");

  _exit(0);
}


void runServer(int listenSocketFD){
  int establishedConnectionFD;
  struct sockaddr_in clientAddress;
  socklen_t sizeOfClientInfo;
  int * childPids = malloc(sizeof(int) * 5);
  int numPids = 0;

  createSocketWatcherThread(&childPids, &numPids);


  while(1){
    while(numPids <= 5){
      sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
    	establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
    	if (establishedConnectionFD < 0){
        perror("ERROR on accept");
      }

      pid_t pid = fork();
      switch(pid){
        case -1:
          perror("HULL BREACH");
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
    perror("ERROR opening socket");
    return 1;
  }


	// Enable the socket to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		perror("ERROR on binding");
	listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections


  if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }

  runServer(listenSocketFD);
	return 0;
}
