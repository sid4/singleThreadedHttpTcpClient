#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
//buffer size for reading from socket
#define BUFFER_SIZE 1000
#define MIN_ARGS 5
//argument for host name
#define ARG_HOSTNAME argv[1]
//argument for port
#define ARG_PORT argv[2]
//argument for connection type
#define ARG_CONN_TYPE argv[3]
//argument for input file
#define ARG_FILE argv[4]
//Max no. of files to be read from a file
#define MAX_FILES 100
//Max characters allowed for a file name
#define MAX_FILE_NAME_SIZE 100
//Max time to wait (in seconds) for chunked response from server in micro seconds
#define MAX_WAIT_FOR_RESPONSE_CHUNK 90000

char **getFileNames(char *file);
char *generateHttpRequestHeader(int persistent,char *filename, char *hostname,int port);
void connect_socket(int port,int *socket_file_descr,char *hostname);
void sendRequestReadResponse(char *http_request,int socket_fd);
/*
 * Prints the error to the standard error stream and exits the program
 */
void signal_error(char *err_msg) {
	fprintf(stderr, err_msg);
	fprintf(stderr, "shutting down");
	exit(1);
}

int main(int argc, char *argv[]) {
	if (argc < MIN_ARGS) {
		if (argc == MIN_ARGS - 1) {
			signal_error(
					"Insufficient arguments. Please enter a file name. Example of proper invocation: ./client localhost 50413 np file.txt");
		}
		else if (argc == MIN_ARGS - 2) {
			signal_error(
					"Insufficient arguments. Please enter a file name and connection type(persistent or non persistent).  Example of proper invocation: ./client localhost 50413 np file.txt");
		}
		else if (argc == MIN_ARGS - 3) {
			signal_error(
					"Insufficient arguments. Please enter a port no of the server to connect to,file name and connection type(persistent or non persistent).  Example of proper invocation: ./client localhost 50413 np file.txt");
		}
		else{
			signal_error(
					"Insufficient arguments. Please enter host name, port no of the server to connect to,file name and connection type(persistent or non persistent).  Example of proper invocation: ./client localhost 50413 np file.txt");
		}

	} else {
		int socket_file_descr,
			port,persistent_connection=(strcmp(ARG_CONN_TYPE,"p")==0)?1:0;

		port = atoi(ARG_PORT);
		//AF_INET is the domain, SOCK_STREAM is the communication style
		connect_socket(port,&socket_file_descr,ARG_HOSTNAME);
		if(persistent_connection){
			char** files_to_retrieve=getFileNames(ARG_FILE);
						//no of files
						int filec=atoi(files_to_retrieve[0]);
						int i=0;
						for(i=1;i<=filec;i++){
							char* http_request;
							if(i<filec){
								//All requests except the last one should be persistent
								http_request=generateHttpRequestHeader(persistent_connection,files_to_retrieve[i],ARG_HOSTNAME,port);
							}
							else{
								//Last request for the persistent connection should be non-peristent
								http_request=generateHttpRequestHeader(!persistent_connection,files_to_retrieve[i],ARG_HOSTNAME,port);
							}
							sendRequestReadResponse(http_request,socket_file_descr);
						}
		}
		else{
			//Nonpersistent Request
			char *http_request=generateHttpRequestHeader(persistent_connection,ARG_FILE,ARG_HOSTNAME,port);
			sendRequestReadResponse(http_request,socket_file_descr);
		}
		//shutdown(socket_file_descr, SHUT_RDWR);
		close(socket_file_descr);
	}
	return 0;
}
/**
 * Sends the HTTP Request to the server and reads the response from the server and prints it on console
 */
void sendRequestReadResponse(char *http_request,int socket_fd){
								//send request
								write(socket_fd,http_request,strlen(http_request));
								free(http_request);
								//setting the max wait time for select
								fd_set set;
								struct timeval max_wait;
								FD_ZERO(&set);
								FD_SET(socket_fd, &set);
								max_wait.tv_sec = 0;
								max_wait.tv_usec = MAX_WAIT_FOR_RESPONSE_CHUNK;
								printf("\n**Response start**\n");
								//could read once over here-iteration2
								struct timeval start, end;
								unsigned long time_taken;
								//start measuring time
								gettimeofday(&start, NULL);
								while(select(socket_fd + 1, &set, NULL, NULL, &max_wait)== 1){
									//iteration2-change it to the server type
									char req_buffer[BUFFER_SIZE]="";
									int readc=read(socket_fd, req_buffer, BUFFER_SIZE - 1);
									if(readc==0){
										//server socket closed, break out of the loop, no more wait required;
										break;
									}
									printf("%s",req_buffer);
									FD_ZERO(&set);
									FD_SET(socket_fd, &set);
									max_wait.tv_sec = 0;
									max_wait.tv_usec = MAX_WAIT_FOR_RESPONSE_CHUNK;
								}
								//end measuring time
								gettimeofday(&end, NULL);
								time_taken = (end.tv_sec - start.tv_sec) * 1000000.0;
								time_taken += (end.tv_usec - start.tv_usec);
								printf("\n**Response end**\n");
								printf("\n**Time taken for receiving the response(micro seconds):%lu\n",time_taken);

}
/**
 * Connect to socket
 */
void connect_socket(int port,int *socket_file_descr,char *hostname){
	int connect_status;
	struct sockaddr_in server;
	struct hostent *server_entry;
	*socket_file_descr = socket(AF_INET, SOCK_STREAM, 0);
			if (*socket_file_descr == -1) {
				signal_error("Failed creating a socket for the client");
			}
			//fetching host information
			server_entry = gethostbyname(hostname);
			if (server_entry == NULL) {
				signal_error("Host not found");
			}
			//clearing the struct
			memset(&server, 0, sizeof(server));
			server.sin_family = AF_INET;
			memmove((char *)&server.sin_addr.s_addr,
					(char *)server_entry->h_addr,
					server_entry->h_length);
			server.sin_port = htons(port);
			connect_status=connect(*socket_file_descr,(struct sockaddr *) &server,sizeof(server));
			if(connect_status<0){
				signal_error("Could not connect to host");
			}
}
/**
 * Returns an array of file names to be read from the given file
 */
char **getFileNames(char *file){
	//filecount
	int filec;
	FILE *input;
	input=fopen(file,"r");
	char **filenames;
	filenames=calloc(MAX_FILES,sizeof (char *));
	if(input==NULL){
		signal_error("Failed to open the input file from the system");
	}
	char line[MAX_FILE_NAME_SIZE]="";
	for(filec=1;fgets(line,MAX_FILE_NAME_SIZE,input)!=NULL;filec++){
		filenames[filec]=calloc(strlen(line)+1,sizeof (char));
		int c=0;
		while(line[c]!='\0'&&line[c]!=EOF&&line[c]!='\n'){
			filenames[filec][c]=line[c];
			c++;
		}
		filenames[filec][c]='\0';
		//clear the line array
		bzero(line,MAX_FILE_NAME_SIZE);
	}
	filenames[0]=calloc(5,sizeof (char));
	snprintf(filenames[0],5,"%d",filec-1);
	//resize the array to have just the right amount of memory to accommodate file names
	//*filenames=realloc(filenames,filec*sizeof(char *));
	fclose(input);
	return filenames;
}
/**
 * Generates HTTP request header
 */
char *generateHttpRequestHeader(int persistent,char *filename, char *hostname,int port){
	char request_header[300] = "";
	snprintf(request_header, sizeof request_header,
				"GET /%s HTTP/1.1\r\nHost: %s:%d\r\nConnection: %s\r\n\r\n",
				filename,
				hostname,
				port,
				persistent?"keep-alive":"close"
			);
	char *request_header_trimmed = (char *) calloc(1, strlen(request_header) + 1);
		{
			int i = 0;
			for (; i < strlen(request_header) + 1; i++) {
				request_header_trimmed[i] = request_header[i];
			}
		}
	return request_header_trimmed;
}
