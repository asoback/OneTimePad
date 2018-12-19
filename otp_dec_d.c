/* 
 * Andrew Soback
 * CS 344
 * otp_dec_d.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues

int main(int argc, char *argv[])
{
	int numChildren = 0;
	pid_t childArray[5];

	int listenSocketFD, establishedConnectionFD, portNumber, charsRead, charsWritten;
	socklen_t sizeOfClientInfo;
	char buffer[1000];
	struct sockaddr_in serverAddress, clientAddress;

	if (argc < 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args

	// Set up the address struct for this process (the server)
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[1]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process

	// Set up the socket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (listenSocketFD < 0) error("ERROR opening socket");

	// Enable the socket to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		error("ERROR on binding");
	listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections

	// Accept a connection, blocking if one is not available until one connects
	sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect

	pid_t pid, cpid;
	int j, exitMethod;
	numChildren = 0;

	while(1){

		//Limit 5 connections
		if(numChildren == 5){
			while(numChildren > 0){
				for(j = 0; j < 5; j++){
					if(childArray[j] != 0){
						cpid = waitpid(childArray[j], &exitMethod, WNOHANG);
						if(exitMethod != 0){ //Status changed
							--numChildren;
							childArray[j] = 0;
						}
					}
				}
			}
		}

		pid = -5;
		establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
		if (establishedConnectionFD < 0) error("ERROR on accept");


		pid = fork();
		switch (pid){
			case -1:
				fprintf(stderr, "err spawning pid\n");
				break;
			case 0:;
				int checkSend;
				checkSend = -5;
				//Send server name
				charsWritten = send(establishedConnectionFD, "otp_dec_d@@", 12, 0); // Send
				if (charsRead < 0) error("ERROR writing to socket");

				//Verify send
				do{
				  ioctl(establishedConnectionFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
				} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
				if (checkSend < 0) error("ioctl error");

				//Get Client name
				char completeMsg[100];
				memset(completeMsg, '\0', sizeof(completeMsg));
				
				do {
					memset(buffer, '\0', sizeof(buffer)); // Clear out the buffer again for reuse
					charsRead = recv(establishedConnectionFD, buffer, sizeof(buffer) -1, 0); // Read data from the socket, leaving \0 at end
					if (charsRead < 0) error("Error reading name from client");
					else{
						strcat(completeMsg, buffer);
					}
				} while(strstr(completeMsg, "@@") == NULL);


				//Test that client is "opt_enc"
				int x = strcmp(buffer, "otp_dec@@");
				if(x != 0){
					fprintf(stderr, "not otp_dec, closing\n", buffer);				
					close(establishedConnectionFD); // Close the existing socket which is connected to the client
					return 1;
				}
				
				//Verification successful
				//Send Ack Msg
				charsWritten = send(establishedConnectionFD, "Ack", 3, 0);

				//Verify send
				do{
				  ioctl(establishedConnectionFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
				} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
				if (checkSend < 0) error("ioctl error");


				//Get Key Size
				memset(buffer, '\0', sizeof(buffer));
				charsRead = recv(establishedConnectionFD, buffer, 255, 0);
				if (charsRead < 0) fprintf(stderr, "Error reading from client");
				int keySize = atoi(buffer);

				//Make arrays
				char *plaintext = malloc(sizeof(char) * keySize);
				char *key = malloc(sizeof(char) * keySize);
				char *ciphertext = malloc(sizeof(char) * keySize);
			
				//Send Ack Msg
				charsWritten = send(establishedConnectionFD, "Ack", 3, 0);

				//Verify send
				do{
				  ioctl(establishedConnectionFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
				} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
				if (checkSend < 0) error("ioctl error");

				//Get ciphertext
				memset(ciphertext, '\0', sizeof(ciphertext));
				int i;
				do {
					memset(buffer, '\0', sizeof(buffer));
					charsRead = recv(establishedConnectionFD, buffer, 999, 0);
					if (charsRead < 0) fprintf(stderr, "Error reading from client");
					if(strstr(buffer, "\n") != NULL){
						for(i = 0; i < 1000; i++){
							if(buffer[i] == '\n'){
								buffer[i] = '\0';
								break;
							}
						}
					}
					strcat(ciphertext, buffer);
				} while(strstr(ciphertext, "@@") == NULL);

				//remove '@@'
				for(i = 0; i < keySize; i++){
					if(ciphertext[i] == '@'){
						ciphertext[i] = '\0';
						ciphertext[i+1] = '\0';
						break;
					}
				}

				//printf("OTP_DEC_D: CipherText: %s\n", ciphertext);

				//Send Ack Msg
				charsWritten = send(establishedConnectionFD, "Ack", 3, 0);
				//Verify send
				do{
				  ioctl(establishedConnectionFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
				} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
				if (checkSend < 0) error("ioctl error");

				//Get Key
				memset(key, '\0', sizeof(key));
				do{
					memset(buffer, '\0', sizeof(buffer));
					charsRead = recv(establishedConnectionFD, buffer, 999, 0);
					if (charsRead < 0) fprintf(stderr, "Error reading from client");
					if(strstr(buffer, "\n") != NULL){
						for(i = 0; i < 1000; i++){
							if(buffer[i] == '\n'){
								buffer[i] = '\0';
							}
						}
					}
					strcat(key, buffer);
				}while(strstr(key, "@@") == NULL);

				//remove '@@'
				for(i = 0; i < keySize; i++){
					if(key[i] == '@'){
						key[i] = '\0';
						key[i+1] = '\0';
						break;
					}
				}

				//Create plaintext
				memset(plaintext, '@', keySize);
				plaintext[keySize-1] = '\0';
				int c, k, p;
				i = 0;
				while(ciphertext[i] != '\0'){

					//Convert to integers A-Z, 0-25, and space as 26
					if(ciphertext[i] == ' '){
						c = 26;
					}
					else{
						c = (int)ciphertext[i] - 65;
					}
					//Repeat for key
					if(key[i] == ' '){
						k = 26;
					}
					else{
						k = (int)key[i] - 65;
					}
					
					//Decryption
					p = (c - k);
					if(p < 0){
						p = 27+p;
					}
					if(p == 26){
						plaintext[i] = ' ';
					}
					else{
						plaintext[i] = (char)(p+65);
					}

					i++;
				}	
		
				//send plaintext
				size_t totalWritten = 0;
				while(totalWritten <  keySize + 2){
					memset(buffer, '\0', sizeof(buffer));
					
					for( i = 0; i < 999; i++){
						if(totalWritten + i <= keySize+2){
							buffer[i] = plaintext[totalWritten + i];
						}
						else{
							buffer[i] = '\n';
							break;
						}
					}

					charsWritten = send(establishedConnectionFD, buffer, strlen(buffer), 0);
					if(charsWritten < 0) error("CLIENT: error writing key to socket");
					if(charsWritten < strlen(buffer)) error("CLIENT: warning, not all data put in socket\n");
					if(charsWritten > 0) totalWritten += charsWritten;

					//Verify send
					do{
					  ioctl(establishedConnectionFD, TIOCOUTQ, &checkSend);  // Check the send buffer for this socket
					} while (checkSend > 0);  // Loop forever until send buffer for this socket is empty
					if (checkSend < 0) error("ioctl error");
					
				}

				close(establishedConnectionFD);

				free(plaintext);
				free(key);
				free(ciphertext);

				return 0;

			default:
				numChildren++;
				int y;
				for(y = 0; y < 5; y++){
					if(childArray[y] == 0){
						childArray[y] = pid;
						break;
					}
				}
				break;
		}
	}

	close(listenSocketFD); // Close the listening socket
	return 0; 
}
