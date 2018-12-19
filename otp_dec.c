/*
 * Andrew Soback
 * CS 344
 * otp_dec.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/ioctl.h>

int checkValid(FILE* fp);
void errorExit(const char *msg){perror(msg); exit(1);}
void error(const char *msg){ perror(msg); }

int main(int argc, char *argv[])
{
	int socketFD, portNumber, charsWritten, charsRead;
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;
    char buffer[1000];

	if (argc < 4) { // Check proper number of arguments
		errorExit("Too few arguments\n");
	 }

	// Open arg 1 and 2 (ciphertext and key)
	FILE *ctxt, *key;
	ctxt = fopen(argv[1], "r");
	key = fopen(argv[2], "r");
	if (ctxt== NULL || key == NULL){
		errorExit("File open error, check filenames\n");
	}

	// Send each to a check valid function, looks for only capital letters A-Z, spaces, 
	// and a new line character as the last character
	// Returns length of text file (-1 for the newline char, is replaced with '@@' in next step)
	int ctxtlen = checkValid(ctxt);
	int keylen = checkValid(key);
	
	// Check to make sure key is larger than plaintext file
	if(ctxtlen > keylen){
		errorExit("Key too small\n");
	}

	// Create arrays to copy the files to
	// Adds two chars to array to account for ending chars
	// Sets all chars to '@' symbol to be marked as the end in network sending and recving
	char ctxtArr[ctxtlen + 2];
	char keyArr[keylen+2];
	char ptxtArr[ctxtlen + 1];
	memset(ctxtArr, '@', ctxtlen+2);
	memset(keyArr, '@', keylen+2);
	memset(ptxtArr, '\0', ctxtlen + 1);

	//return to start of file 
	//Help from https://stackoverflow.com/a/32366729
	fseek(ctxt, 0, SEEK_SET);
	fseek(key, 0, SEEK_SET);

	// Copy ciphertext file to array
	int i;
	for(i = 0; i < ctxtlen; i++){
		ctxtArr[i] = getc(ctxt);
	}

	// Copy key file to array
	for(i = 0; i < keylen; i++){
		keyArr[i] = getc(key);
	}

	// Close files, no longer needed
	fclose(ctxt);
	fclose(key);

	// Set up the server address struct
	memset((char*)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[3]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverHostInfo = gethostbyname("localhost"); // Convert the machine name into a special form of address
	if (serverHostInfo == NULL) { fprintf(stderr, "Client error, no such host\n"); exit(1); }
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

	// Set up the socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (socketFD < 0){
		fprintf(stderr, "CLIENT: ERROR opening socket\n");
		exit(1);
	}
	
	// Connect to server
	if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){ // Connect socket to address
		fprintf(stderr, "CLIENT: ERROR connecting\n");
		exit(1);
	}

	/* Begin communication with server */

	int checkSend = -5;
	int checkCmp;

	//Recv name of server
	char completeMsg[100];
	memset(completeMsg, '\0', sizeof(completeMsg));

	do {
		memset(buffer, '\0', sizeof(buffer)); // Clear out the buffer again for reuse
		charsRead = recv(socketFD, buffer, sizeof(buffer) -1, 0); // Read data from the socket, leaving \0 at end
		if (charsRead < 0) error("CLIENT: ERROR reading from socket");
		else{
			strcat(completeMsg, buffer);
		}
	} while(strstr(completeMsg, "@@") == NULL);

	//Validate
	checkCmp = strcmp(buffer, "otp_dec_d@@");
	if(checkCmp != 0){
		fprintf(stderr, "Not otp_dec_d\n");
		close(socketFD);
		return 1;
	}

	// Send name of client
	memset(buffer, '\0', sizeof(buffer));
	strcpy(buffer, "otp_dec@@"); 
	charsWritten = send(socketFD, buffer, strlen(buffer), 0);
	if(charsWritten < 0){
		fprintf(stderr, "CLIENT: error writing to socket\n");
	}
	if(charsWritten < strlen(buffer)){
		fprintf(stderr, "CLIENT: warning, not all data put in socket\n");
	}

	//Verify send
	do{
	  ioctl(socketFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
	} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
	if (checkSend < 0) error("ioctl error");

	//Recv Ack msg
	memset(buffer, '\0', sizeof(buffer));
	charsRead= recv(socketFD, buffer, sizeof(buffer) -1, 0);
	if (charsRead < 0) error("CLIENT: ERROR reading from socket");

	// Send key size
	memset(buffer, '\0', sizeof(buffer));
	sprintf(buffer, "%d", (keylen+2));	
	charsWritten = send(socketFD, buffer, strlen(buffer), 0);
	if(charsWritten < 0) error("CLIENT: error writing to socket");
	if(charsWritten < strlen(buffer)) printf("CLIENT: warning, not all data put in socket\n");

	//Verify send
	do{
	  ioctl(socketFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
	} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
	if (checkSend < 0) error("ioctl error");

	// Recv Ack msg
	memset(buffer, '\0', sizeof(buffer));
	charsRead= recv(socketFD, buffer, sizeof(buffer) -1, 0);
	if (charsRead < 0) error("CLIENT: ERROR reading from socket");

	//Send ciphertext
	size_t totalWritten = 0;

	while(totalWritten < ctxtlen + 2){
		memset(buffer, '\0', sizeof(buffer));

		for( i = 0; i < 999; i++){
			if(totalWritten + i <= ctxtlen+2){
				buffer[i] = ctxtArr[totalWritten + i];
			}
			else{
				buffer[i] = '\n';
				break;
			}
		}

		charsWritten = send(socketFD, buffer, strlen(buffer), 0);
		if(charsWritten < 0) error("CLIENT: error writing plaintext to socket");
		if(charsWritten < strlen(buffer)) error("CLIENT: warning, not all data put in socket\n");
		if(charsWritten > 0) totalWritten += charsWritten;

		//Verify send
		do{
		  ioctl(socketFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
		} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
		if (checkSend < 0) error("ioctl error");
	}
	

	//Recv Ack
	memset(buffer, '\0', sizeof(buffer));
	charsRead= recv(socketFD, buffer, sizeof(buffer) -1, 0);
	if (charsRead < 0) error("CLIENT: ERROR reading from socket");

	//Send Key
	totalWritten = 0;
	while(totalWritten < keylen + 2){
		memset(buffer, '\0', sizeof(buffer));
		
		for( i = 0; i < 999; i++){
			if(totalWritten + i <= keylen+2){
				buffer[i] = keyArr[totalWritten + i];
			}
			else{
				buffer[i] = '\n';
				break;
			}
		}

		charsWritten = send(socketFD, buffer, strlen(buffer), 0);
		if(charsWritten < 0) error("CLIENT: error writing key to socket");
		if(charsWritten < strlen(buffer)) error("CLIENT: warning, not all data put in socket\n");
		if(charsWritten > 0) totalWritten += charsWritten;

		//Verify send
		do{
		  ioctl(socketFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
		} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
		if (checkSend < 0) error("ioctl error");

	}

	//Recv dencrypted text	
	size_t totalRead = 0;
	while(totalRead <  ctxtlen + 2){
		memset(buffer, '\0', sizeof(buffer));
		charsRead= recv(socketFD, buffer, sizeof(buffer) -1, 0);
		if (charsRead < 0) error("CLIENT: ERROR reading from socket");
		else{
			for(i = 0; i < 999; i++){
				if(buffer[i] == '\n'){
					buffer[i] = '\0';
					break;
				}
			}
			strcat(ptxtArr, buffer);
			totalRead += charsRead;
		}
	}
	if(strstr(ptxtArr, "@@") != NULL){
		for(i = 0; i < ctxtlen + 2; i++){
			if(ptxtArr[i] == '@'){
				ptxtArr[i] = '\0';
			}
		}
	}
	// Print plaintext to stdout
	printf("%s\n", ptxtArr);

	close(socketFD); // Close the socket
	return 0;
}

int checkValid(FILE *fp){
	int c;
	int count = 0;
	while((c = getc(fp)) != EOF){
		count++;
		if(c != 32 && ( c <65 || c > 90 )){
			if(c == 10){ //This is a newline
				//check that it is the last character
				c = getc(fp);
				if (c == EOF){
					return (count - 1); //This is to make it the correct length, when copying to the array in main.
				}
				//invalid
				fprintf(stderr, "Invalid characters found in file.\n");
				exit(1);
			}
		}
	}
	return count;
}
