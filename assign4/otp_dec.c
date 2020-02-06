#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(const char *msg) { fprintf(stderr, msg); exit(0); } // Error function used for reporting issues

int main(int argc, char *argv[]){
	int socketFD, portNumber, charsWritten, charsRead;
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;
	char buffer[70001];
	char message[140002];

	if (argc != 4) error("CLIENT: ERROR, Incorrect usage. Try: otp_dec key message port \n"); // Check usage & args


	// Set up the server address struct
	memset((char*)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[3]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverHostInfo = gethostbyname("localhost"); // Convert the machine name into a special form of address
	if (serverHostInfo == NULL) { fprintf(stderr, "CLIENT: ERROR, no such host\n"); exit(0); }
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

	// Set up the socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (socketFD < 0) error("CLIENT: ERROR opening socket");

	// Connect to server
	if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to address
		error("CLIENT: ERROR connecting");

		// Get input message from user
		memset(buffer, '\0', sizeof(buffer)); // Clear out the buffer array

		// Get encoded message
		FILE *fp = fopen(argv[1], "r");
		fgets(buffer, sizeof(buffer), fp);
		fclose(fp);

		// Cleans up text
		buffer[strcspn(buffer, "\n")] = '\0';
		// Adds message to main send string
		strcpy(message, buffer);
		int messageLen = strlen(buffer);  // Now message holds raw message

		// Add delimiter
		strcat(message, "@");
		memset(buffer, '\0', sizeof(buffer));

		// Get key
		FILE *fp2 = fopen(argv[2], "r");
		fgets(buffer, sizeof(buffer), fp2);
		fclose(fp2);

		// Add key to main send string
		buffer[strcspn(buffer, "\n")] = '\0';
		strcat(message, buffer);

		int keyLen = strlen(buffer);

		if (keyLen < messageLen){
			fprintf(stderr,"Error: key '%s' is too short\n", argv[2]);
			exit(0);
		}

		// Creathes handshake string once big fileIO is done
		char buff[20];
		memset(buffer, '\0', sizeof(buffer));
		strcpy(buffer, "otp_dec");
		strcat(buffer, "@");
		sprintf(buff,"%d", (keyLen + 1) + messageLen);
		strcat(buffer, buff);

	//printf(buffer);
	//printf("\n");
	//printf(message);

	buffer[strcspn(buffer, "\n")] = '\0';
	// Send message to server
	charsWritten = send(socketFD, buffer, strlen(buffer), 0); // Write to the server
	if (charsWritten < 0) error("CLIENT: ERROR writing to socket\n");
	if (charsWritten < strlen(buffer)) error("CLIENT: WARNING: Not all data written to socket!\n");

	// Get return message from server
	memset(buffer, '\0', sizeof(buffer)); // Clear out the buffer again for reuse
	charsRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0); // Read data from the socket, leaving \0 at end
	if (charsRead < 0) error("CLIENT: ERROR reading from socket");
	//printf("CLIENT: I received this from the server: \"%s\"\n", buffer);
	fflush(stdout);

  if( strstr(buffer, "HS")){

    charsWritten = send(socketFD, message, strlen(message), 0); // Write to the server
  	if (charsWritten < 0) error("CLIENT: ERROR writing to socket");
  	if (charsWritten < strlen(buffer)) printf("CLIENT: WARNING: Not all data written to socket!\n");

    memset(buffer, '\0', sizeof(buffer)); // Clear out the buffer again for reuse
		while (recv(socketFD, buffer, sizeof(buffer), MSG_PEEK) < messageLen){
	    usleep(500);
	  }
	  if (recv(socketFD, buffer, sizeof(buffer), 0) != messageLen ){
	    error("Failed to transfer bytes...");
	  }
  	//printf("CLIENT: I received this from the server: \"%s\"\n", buffer);
  	//fflush(stdout);
		printf("%s\n", buffer);
  }
  else{
    error("You may not use this program with the supplied server.\n");
  }

	close(socketFD); // Close the socket
	return 0;
}
