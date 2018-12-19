/* 
 * Andrew Soback
 * CS 344
 * otp_enc.c
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

void error(const char *msg) { perror(msg); } // Error function used for reporting issues
int checkValid(FILE* fp);

int main(int argc, char *argv[])
{
	int socketFD, portNumber, charsWritten, charsRead;
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;
	char buffer[1000];
    
	if (argc < 4) { fprintf(stderr,"USAGE: %s hostname port\n", argv[0]); exit(1); } // Check usage & args

	//open arg 1 and 2 (files and key)
	FILE *ptxt, *key;
	ptxt = fopen(argv[1], "r");
	key = fopen(argv[2], "r");
	if (ptxt== NULL || key == NULL){
		fprintf(stderr, "Failed to open file\n");
		exit(1);
	}
	
	// Check to ensure that the text is valid, as well as getting word count (-1 to remove newline)
	int ptxtlen = checkValid(ptxt);
	int keylen = checkValid(key);

	//Check to make sure key is larger than plaintext file
	if(ptxtlen > keylen){
		fprintf(stderr, "Key is too small\n");
		exit(1);
	}

	//Copies files to array

	//Create arrays, set to all @ symbols for transfer
	char ptxtArr[ptxtlen + 3];
	char keyArr[keylen+3];
	memset(ptxtArr, '@', ptxtlen+2);
	memset(keyArr, '@', keylen+2);
	ptxtArr[ptxtlen+2] = '\0';
	keyArr[keylen + 2] = '\0';
	
	char ctxtArr[ptxtlen+1];
	memset(ctxtArr, '\0', ptxtlen+1);

	//return to start of file 
	//Help from https://stackoverflow.com/a/32366729
	fseek(ptxt, 0, SEEK_SET);
	fseek(key, 0, SEEK_SET);

	int i;
	for(i = 0; i < ptxtlen; i++){
		ptxtArr[i] = getc(ptxt);
	}
	
	for(i = 0; i < keylen; i++){
		keyArr[i] = getc(key);
	}

	fclose(ptxt);
	fclose(key);

	// Set up the server address struct
	memset((char*)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[3]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverHostInfo = gethostbyname("localhost"); // Convert the machine name into a special form of address
	if (serverHostInfo == NULL) { fprintf(stderr, "CLIENT: ERROR, no such host\n"); exit(1); }
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

	// Set up the socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
		if (socketFD < 0){
			error("CLIENT: ERROR opening socket");
			exit(1);
		}

	// Connect to server
	if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){ // Connect socket to address
		error("CLIENT: ERROR connecting");
		exit(1);
	}

	/* Begin communication */

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
	checkCmp = strcmp(buffer, "otp_enc_d@@");
	if(checkCmp != 0){
		fprintf(stderr, "Not otp_enc_d\n");
		close(socketFD);
		return 1;
	}

	//Send name of client
	memset(buffer, '\0', sizeof(buffer));
	strcpy(buffer, "otp_enc@@"); 
	charsWritten = send(socketFD, buffer, 9, 0);
	if(charsWritten < 0) error("CLIENT: error writing to socket");
	if(charsWritten < strlen(buffer)) error("CLIENT: warning, not all data put in socket\n");

	//Verify send
	do{
	  ioctl(socketFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
	} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
	if (checkSend < 0) error("ioctl error");

	//Recv Ack msg
	memset(buffer, '\0', sizeof(buffer));
	charsRead= recv(socketFD, buffer, sizeof(buffer) -1, 0);
	if (charsRead < 0) error("CLIENT: ERROR reading from socket");

	//Send key size
	memset(buffer, '\0', sizeof(buffer));
	sprintf(buffer, "%d", (keylen+2));	
	charsWritten = send(socketFD, buffer, strlen(buffer), 0);
	if(charsWritten < 0) error("CLIENT: error writing to socket");
	if(charsWritten < strlen(buffer)) error("CLIENT: warning, not all data put in socket\n");

	//Verify send
	do{
	  ioctl(socketFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
	} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
	if (checkSend < 0) error("ioctl error");

	//Recv Ack msg
	memset(buffer, '\0', sizeof(buffer));
	charsRead= recv(socketFD, buffer, 3, 0);
	if (charsRead < 0) error("CLIENT: ERROR reading from socket");

	//Send plaintext
	size_t totalWritten = 0;

	while(totalWritten < ptxtlen + 2){
		memset(buffer, '\0', sizeof(buffer));

		for( i = 0; i < 999; i++){
			if(totalWritten + i <= ptxtlen+2){
				buffer[i] = ptxtArr[totalWritten + i];
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
	charsRead= recv(socketFD, buffer, 3, 0);
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

	//Recv encrypted text	
	size_t totalRead = 0;
	while(totalRead < ptxtlen + 2){
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
			strcat(ctxtArr, buffer);
			totalRead += charsRead;
		}
	}
	if(strstr(ctxtArr, "@@") != NULL){
		for(i = 0; i < ptxtlen + 2; i++){
			if(ctxtArr[i] == '@'){
				ctxtArr[i] = '\0';
			}
		}
	}
	//Output encrypted text
	printf("%s\n", ctxtArr);

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
				fprintf(stderr, "Invalid characters found in file\n");
				exit(1);
			}
		}
	}
	return count;
}
