/*
http-server.c
a program by Jack Damon
April, 2017
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#define MAXPENDING 5

static void die(const char *message)
{
	perror(message);
	exit(1);
}

int sendStatus(int clientSock, char *sendMessage);
int sendResponse(int clientSock, char *responseMessage);
int isDir(char *filepath);
int sendMdbResults(int client_sock, int mdb_lookup_sock, FILE *response, char *toSearch);


// ./http-server <server_port> <web_root> <mdb-lookup-host> <mdb-lookup-port>
// example call: ./http-server 8888 ~/html localhost 9999

int main(int argc, char **argv)
{
	if (argc != 5){
		fprintf(stderr, "%s\n", "usage: http-server <server_port> <web_root> <mdb-lookup-host> <mdb-lookup-port>");
		exit(1);
	}

// Make TCP connection to mdb-lookup-server

	struct addrinfo hints, *res;
        int mdb_lookup_sock;

        // Loading up address structs with getaddrinfo()

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        getaddrinfo(argv[3], argv[4], &hints, &res);

        // make a socket:

        mdb_lookup_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if(mdb_lookup_sock < 0){
                die("socket() failed");
        }

        int connectCheck;
        connectCheck = connect(mdb_lookup_sock, res->ai_addr, res->ai_addrlen);
        if(connectCheck < 0){
                die("connect() failed");
        }


	FILE *response = fdopen(mdb_lookup_sock, "r");

	freeaddrinfo(res);

// Open socket on a port
	int server_sock, client_sock;
	struct sockaddr_in echoServAddr, echoClntAddr;
	unsigned short echoServPort = atoi(argv[1]);
	unsigned int clntLen;

	/* Create a socket for incoming connections */
	if ((server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		die("socket() failed");
	}

	/* Construct local address structure */
	memset(&echoServAddr, 0, sizeof(echoServAddr));
	echoServAddr.sin_family = AF_INET;
	echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	echoServAddr.sin_port = htons(echoServPort);

	/* Bind to the local address */
	if (bind(server_sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0){
		die("bind() failed"); // CONTINUE TO BREAK LOOP
	}

	/* Mark the socket so it will listen for incoming connections */
	if (listen(server_sock, MAXPENDING) < 0){
		die("listen() failed");
	}

	while(1){
	// wait for client to make a request
		/* Set the size of the in-out parameter */
		clntLen = sizeof(echoClntAddr);

		/* Wait for a client to connect */
		if((client_sock = accept(server_sock, (struct sockaddr *) &echoClntAddr, &clntLen)) < 0){
			die("accept() failed");
		}

		/* clntSock is connected to a client */

		char *ip = inet_ntoa(echoClntAddr.sin_addr);

	// read the request (eg, this person wants page "index.html")
		char recvBuffer[2048];
		if (recv(client_sock, &recvBuffer, sizeof(recvBuffer), 0) < 0){
			char *returnMessage = "500 Internal Service Error";
			sendResponse(client_sock, returnMessage);

			printf("Connected from %s, died before recieving request. %s\n", ip, returnMessage);
			continue;
		}


// Parse and check the request
		
	//First, fix the request URI if it needs it

		char *token_separators = "\t \r\n"; // tab, space, new line
                char *method = strtok(recvBuffer, token_separators);
                char *requestURI = strtok(NULL, token_separators);
		char *httpVersionGrabber = strtok(NULL, token_separators);
		
		char httpVersion[1024];

		if (requestURI == NULL){
                        char *returnMessage = "400 Bad Request";
                        sendResponse(client_sock, returnMessage);

                        // Print request/response details to stdout
                        printf("%s \"    \" %s\n", ip, returnMessage);

                        close(client_sock);
                        continue;
                }


		if(httpVersionGrabber != NULL){
			strcpy(httpVersion, httpVersionGrabber);
		}

		if(!strstr(requestURI, "mdb-lookup")){
	
			char pathToCheck[1024];
			strcpy(pathToCheck, argv[2]);
			strcat(pathToCheck, requestURI);
			int dirCheck;

			if(pathToCheck[strlen(pathToCheck)-1] != '/'){

				if((dirCheck = isDir(pathToCheck))){
					strcat(requestURI, "/index.html");	
				}
				if (dirCheck < 0){
					char *returnMessage = "404 Not Found";
					sendResponse(client_sock, returnMessage);
					close(client_sock);
					continue;
				}
			
			}

			if(requestURI[strlen(requestURI) -1] == '/'){
				strcat(requestURI, "index.html");
			}
		
			if(!strstr(method, "GET")){

				char *returnMessage = "501 Not Implemented";
				sendResponse(client_sock, returnMessage);

				// Print request/response details to stdout
				printf("%s \"%s %s %s\" %s\n", ip, method, requestURI, httpVersion, returnMessage);

				close(client_sock);
				continue;	
			
			}

			// Respond with 400 if request URI does not begin with "/"
			if(requestURI[0] != '/'){
				
				char *returnMessage = "400 Bad Request";
				sendResponse(client_sock, returnMessage);

				// Print request/response details to stdout
				printf("%s \"%s %s %s\" %s\n", ip, method, requestURI, httpVersion, returnMessage);

				close(client_sock);
				continue;

			}			

			// Respond with 400 if request URI contains "/.."
			if(strstr(requestURI,"/..")){
			
				char *returnMessage = "400 Bad Request";
				sendResponse(client_sock, returnMessage);

				// Print request/response details to stdout
				printf("%s \"%s %s %s\" %s\n", ip, method, requestURI, httpVersion, returnMessage);

				close(client_sock);
				continue;

				
			} 


			// Respond with 501 if protocol is not HTTP
			if(!strstr(httpVersion,"HTTP")){
			
				char *returnMessage = "501 Not Implemented";
				sendResponse(client_sock, returnMessage);

				// Print request/response details to stdout
				printf("%s \"%s %s %s\" %s\n", ip, method, requestURI, httpVersion, returnMessage);

				close(client_sock);
				continue;

			}
			

			// Respond with 501 if HTTP version is not 1.0 or 1.1
			if( (!strstr(httpVersion,"1.0")) && (!strstr(httpVersion,"1.1")) ){ 
				
				char *returnMessage = "501 Not Implemented";
				sendResponse(client_sock, returnMessage);

				// Print request/response details to stdout
				printf("%s \"%s %s %s\" %s\n", ip, method, requestURI, httpVersion, returnMessage);

				close(client_sock);
				continue;
			}
			
			char filepath[1024];
			strcpy(filepath, argv[2]);
			strcat(filepath, requestURI);

		// find, read, and send "index.html"

			FILE *toRead = fopen(filepath, "rb");
			if (toRead == NULL){

				char *returnMessage = "404 Not Found"; 
				sendResponse(client_sock, returnMessage);

				// Print request/response details to stdout
				printf("%s \"%s %s %s\" %s\n", ip, method, requestURI, httpVersion, returnMessage);
				close(client_sock);
				continue;
			} 
			
			char buffer[4096];
			size_t n;
			char *statusMessage = "200 OK";
			while((n = fread(buffer, 1, sizeof(buffer), toRead)) > 0){
				if(send(client_sock, buffer, n, 0) < 0){
					die("send() failed");
				}
			}

			printf("%s \"%s %s %s\" %s\n", ip, method, requestURI, httpVersion, statusMessage);	

			close(client_sock);
			fclose(toRead);
		} else {
				const char *form =
				    "HTTP/1.0 200 OK\n"
				    "Content-Type: text/html\r\n\r\n"
				    "<h1>mdb-lookup</h1>\n"
				    "<p>\n"
				    "<form method=GET action=/mdb-lookup>\n"
				    "lookup: <input type=text name=key>\n"
				    "<input type=submit>\n"
				    "</form>\n"
				    "<p>\n";
				
				int len, bytes_sent;
                                len = strlen(form);
				
				bytes_sent = send(client_sock, form, len, 0);

				if(bytes_sent != strlen(form)){
					die("value returned by send() doesn't match value in len");
       				}
				if (bytes_sent < 0){
					die("send() failed");
				}

				if(strstr(requestURI, "mdb-lookup?key=")){
					char reqCopy[1024];
					strcpy(reqCopy, requestURI);

					char *token_separators = "=";
                                        char *firstHalf = strtok(requestURI, token_separators);
                                        char *wordToSearch = strtok(NULL, token_separators);
		
					int searchIsNull = 0;	
					if(wordToSearch == NULL){
						searchIsNull = 1;
					}

					char *returnMessage = "200 OK";

					if(searchIsNull){

						char *newLineSearch = "\n";

						printf("looking up[%s]: %s \"%s %s %s\" %s\n", "", ip, method, reqCopy, httpVersion, returnMessage);
                                                 
                                                sendMdbResults(client_sock, mdb_lookup_sock, response, newLineSearch);
                                                 
                                                if(firstHalf){
                                                        int x = 1;
                                                        x--;
                                                }


					} else {

						printf("looking up[%s]: %s \"%s %s %s\" %s\n", wordToSearch, ip, method, reqCopy, httpVersion, returnMessage);
                                        
        	                                sendMdbResults(client_sock, mdb_lookup_sock, response, wordToSearch);
                                        
                	                        if(firstHalf){
                        	                        int x = 1;
                                	                x--;
                                        	}

					}

				} else {
					char *returnMessage = "200 OK";
					printf("%s \"%s %s %s\" %s\n", ip, method, requestURI, httpVersion, returnMessage);	
				}
				close(client_sock); 		
		}
	}
	// send an html header, THEN send contents of "index.html"
	fclose(response);
	return 0;
}

int sendMdbResults(int client_sock, int mdb_lookup_sock, FILE *response, char *toSearch)
{
	if(strcmp(toSearch, "\n")){
		strcat(toSearch, "\n");
	}

	int len, bytes_sent;
	len = strlen(toSearch);

	bytes_sent = send(mdb_lookup_sock, toSearch, len, 0);
	if(bytes_sent != strlen(toSearch)){
                die("value returned by send() doesn't match value in len");
        }
        if (bytes_sent < 0){
                die("send() failed");
        }
	
	char responseBuf[1024] = { 0 };
	
	char *tableOpenTags = "<table border><tbody>";
	len = strlen(tableOpenTags);

	bytes_sent = send(client_sock, tableOpenTags, len, 0);

	if(bytes_sent != strlen(tableOpenTags)){
		die("value returned by send() doesn't match value in len");
	}
	if (bytes_sent < 0){
		die("send() failed");
	}

	int color = 0;
	char *s;

	while((s = fgets(responseBuf, sizeof(responseBuf), response)) != NULL){
		if(strlen(s) == 1){
			break;
		}

		char tableItem[2048];

		if(color == 0){
			color = 1;
			sprintf(tableItem,  "<tr><td>%s</td></tr>", responseBuf);
		} else {
			color = 0;
			sprintf(tableItem, "<tr><td bgcolor=\"yellow\">%s</td></tr>", responseBuf);
		}

		int len, bytes_sent;
		len = strlen(tableItem);
		
		bytes_sent = send(client_sock, tableItem, len, 0);

		if(bytes_sent != strlen(tableItem)){
			die("value returned by send() doesn't match value in len");
		}
		if (bytes_sent < 0){
			die("send() failed");
		}


	}
	
	char *tableCloseTags = "</tbody></table>";
        len = strlen(tableCloseTags);

        bytes_sent = send(client_sock, tableCloseTags, len, 0);

        if(bytes_sent != strlen(tableCloseTags)){
                die("value returned by send() doesn't match value in len");
        }
        if (bytes_sent < 0){
                die("send() failed");
        }
	
	return 0;
}

/*
** Checks whether passed in filepath is a valid directory or not. On success returns positive non-zero, on failure returns -1.
*/ 
int isDir(char *filepath)
{
                 struct stat fileStat;
                 if(stat(filepath, &fileStat) != 0){
                 	return -1;
		 }
                 return S_ISDIR(fileStat.st_mode);
}

/*
** Sends styilized response to indicated client socket. On success returns number of bytes sent, on failure returns -1.
*/
int sendResponse(int client_sock, char *responseMessage)
{
	char response[1024];
	sprintf(response, "HTTP/1.0 %s\r\nContent-Type: text/html\r\n\r\n<html><body><h1> %s </h1></body></html>\r\n\r\n", responseMessage, responseMessage);
	
	int len, bytes_sent;
	len = strlen(response);
	
	bytes_sent = send(client_sock, response, len, 0);

	if(bytes_sent != strlen(response)){
                die("value returned by send() doesn't match value in len");
        }
        if (bytes_sent < 0){
                die("send() failed");
        }

	return bytes_sent;
}

/*
** Sends status message to indicated client socket. On success returns number of bytes sent, on failure returns -1. 
*/
int sendStatus(int client_sock, char *sendMessage)
{
	// Send return message back to client
	int len, bytes_sent;
	len = strlen(sendMessage);
	bytes_sent = send(client_sock, sendMessage, len, 0);
	
	return(bytes_sent);
}
