/*---------------------------------------------------------------------------------------
--	SOURCE FILE:		client.c - A simple file transfer system
--
--	PROGRAM:		client.exe
--
--	FUNCTIONS:
--		int main (int argc, char **argv);
--		void client(int sd);
--		void sendCommand(char *sbuf, int sd);
--		void getCommand();
--		void startServer(char *command, char *filename, int fileLength, char *line, int clientSD);
--		void clientListen (int	sd, char *command, char *filename, int fileLength, char *line, int clientSD);
--		int readServer(int new_sd, struct sockaddr_in client, char *command, char *filename, int fileLength);
--
--	DATE:			October 1, 2018
--
--	REVISIONS:		(Date and Description)
--
--	DESIGNERS:		Haley Booker
--
--	PROGRAMMERS:	Haley Booker
--
--	NOTES:
--	The client program of a file transfer system. The client requires a host
--	ip address of the server. It will attempt to connect to the server. If it
--	connects successfully, it will wait for input from the user.
-- 	If the user enters SEND [filename], the program will search for the file. If
--	it finds the file, it gets the filelength and starts the client server.
--	Once the clients server is listening it sends the server program
--	SEND [filename] [filelength]. When the server sends a connection request,
--	the client sends the file. When the file is finished the client closes
--	the client server, and waits for user input.
--	If the the user enters GET [filename], the client sends the command to
--	the server. When it recieves GET [filename] [filelength], it will start
--	the client server. After the client server is started is sends
--	START [filename] [filelength] to the server. The server will open a socket
--	and connect to the client server. Once the server is connected it will send
--	the file to the client. If the doesn't exist, it'll be created. If it
--	does exist it'll be overwritten.
--	If the user enters CLOSE, the client sends the server CLOSE and then exits
--	the program.
--	If the user enters EXIT the client sends the server EXIT and then exits
--	the program.
---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/sendfile.h>
#include <math.h>
#include "router_packet.h"
#include <sys/select.h>
#include <sys/time.h>

#define SERVER_UDP_PORT		7005	// Default port
#define MAXLEN			2000  	// Buffer length
#define TRUE	1
void clientConnection(int sd, struct sockaddr_in server, struct sockaddr_in client);
void sendCommand(char *sbuf, int sd, struct sockaddr_in server, struct sockaddr_in client);
void startServer(char *command, char *filename, int fileLength, char *line, int clientSD, struct sockaddr_in server,struct sockaddr_in client);
void clientListen (int	sd, char *command, char *filename, int fileLength, char *line, int clientSD, struct sockaddr_in server, struct sockaddr_in client,struct sockaddr_in clientServer);
int readServer(int new_sd, struct sockaddr_in client, char *command, char *filename, int fileLength,struct sockaddr_in clientServer);
char  *host;
void packetGen(char * line, int fileLength, struct sockaddr_in server, struct sockaddr_in client);
void genPacketStruct(char * buffer);
int countNewLine(FILE * fp);

struct packetStruct *packetS;
char packet[MAXLEN];
char * localIPBuff;
char localIP[16];
int seqNum;
int ackNum;
/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: main
--
--	DATE:			October 1, 2018
--
-- REVISIONS: (Date and Description)
-- N/A
--
-- DESIGNER: Haley Booker
--
--	PROGRAMMERS:	Haley Booker
--
-- INTERFACE: int main (int argc, char **argv)
-- int argc: The number of command line arguments
--	char **argv: The command line arguments
--
-- RETURNS: An integer used to define if an error occurs
--
-- NOTES:
-- The start point of the program. The function takes the server ip address
-- from the command line arguments and will attempt to connect to the server.
-- If the connection is successful, the client function is called.
----------------------------------------------------------------------------------------------------------------------*/
int main (int argc, char **argv)
{
	int sd, port;
	struct hostent	*hp;
	struct sockaddr_in server;
	char **pptr;
	char str[16];
	char hostbuffer[256];

	gethostname(hostbuffer, sizeof(hostbuffer));
	packetS = (struct packetStruct * )malloc(sizeof(struct packetStruct));

    struct hostent *host_entry;
	struct	sockaddr_in client;
	host_entry = gethostbyname(hostbuffer);
	localIPBuff = inet_ntoa(*((struct in_addr*)
    host_entry->h_addr_list[0]));
	strcpy(localIP,localIPBuff);
    seqNum = 0;
    ackNum = 0;
	switch(argc)
	{
		case 2:
			host =	argv[1];	// Host name
			port =	SERVER_UDP_PORT;
		break;
		case 3:
			host =	argv[1];
			port =	atoi(argv[2]);	// User specified port
		break;
		default:
			fprintf(stderr, "Usage: %s host [port]\n", argv[0]);
			exit(1);
	}

    // Create a datagram socket
	if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror ("Can't create a socket\n");
		exit(1);
	}

    // Store server's information
	bzero((char *)&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_UDP_PORT);

	if ((hp = gethostbyname(host)) == NULL)
	{
		fprintf(stderr,"Can't get server's IP address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);

	// Bind local address to the socket
	bzero((char *)&client, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(port);
	client.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sd, (struct sockaddr *)&client, sizeof(client)) == -1)
	{
		perror ("Can't bind name to socket");
		exit(1);
	}

	printf("Connected:    Server Name: %s\n", hp->h_name);
	pptr = hp->h_addr_list;
	printf("\t\tIP Address: %s\n", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));
	clientConnection(sd, server,client);
	free(packetS);
	return 0;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: client
--
--	DATE:			October 1, 2018
--
-- REVISIONS: (Date and Description)
-- N/A
--
-- DESIGNER: Haley Booker
--
--	PROGRAMMERS:	Haley Booker
--
-- INTERFACE: void client(int sd)
-- int sd: The socket descriptor
--
-- RETURNS: void
--
-- NOTES:
-- Goes into a forever loop waiting for user input.
-- If the user types SEND [filename], the sendCommand funciton is called.
-- If the user types GET [filename], the program sends the server
-- GET [filename] and waits for the server to respond. Once the server
-- sends back GET [filename] [filelength], the getCommand function is called.
-- If the user types CLOSE, the program sends the server CLOSE and returns.
-- If the user types EXIT, the program sends the server EXIT and returns.
----------------------------------------------------------------------------------------------------------------------*/
void clientConnection(int sd, struct sockaddr_in server, struct sockaddr_in client)
{
	char sbuf[MAXLEN];
	char s[5];
	char space[2];
	strcpy(s, "SEND");
	strcpy(space, " ");
    socklen_t server_len;

    char	*bp;
	int	n, bytes_to_read;
    char	buf[MAXLEN];
    server_len = sizeof(server);

    //Three way handshake
    packetGen("SYN", 0, server, client);
    if (sendto (sd, packet, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
    {
        perror("sendto failure");
        exit(1);
    }

	bp = buf;
    bytes_to_read = MAXLEN;
    //SYNACK
    while ((n = recvfrom (sd, bp, MAXLEN, 0, (struct sockaddr *)&server, &server_len)) < bytes_to_read)
    {
        bp += n;
        bytes_to_read -= n;
    }
	printf("%s\n", buf);
    genPacketStruct(buf);
    if (strncmp("SYNACK",packetS->data, 6) == 0)
    {
        packetGen("ACK", 0, server, client);
        if (sendto (sd, packet, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
        {
            perror("sendto failure");
            exit(1);
        }
    }

	while (TRUE)
	{
		printf("Transmit:\n");
		//gets(sbuf); // get user's text
		fgets (sbuf, MAXLEN, stdin);

		// Transmit data through the socket
		if (strncmp("SEND",sbuf, 4) == 0)
		{
			sendCommand(sbuf, sd, server,client);
		}
		if (strncmp("CLOSE",sbuf, 5) == 0)
		{
			packetGen(sbuf, 0, server, client);
            if (sendto (sd, packet, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
            {
                perror("sendto failure");
                exit(1);
            }
			close (sd);
			return;
		}
		if (strncmp("EXIT",sbuf, 4) == 0)
		{
			packetGen(sbuf, 0, server, client);
            if (sendto (sd, packet, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
            {
                perror("sendto failure");
                exit(1);
            }
			close (sd);
			return;
		}
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: sendCommand
--
--	DATE:			October 1, 2018
--
-- REVISIONS: (Date and Description)
-- N/A
--
-- DESIGNER: Haley Booker
--
--	PROGRAMMERS:	Haley Booker
--
-- INTERFACE: void sendCommand(char *sbuf, int sd)
-- char *sbuf: The command line "SEND [filename] [filelength]"
-- int sd: The socket descriptor
--
-- RETURNS: void
--
-- NOTES:
-- Removes the added newline character to the user input string. Searches for the file. If
-- it finds the file, it gets the filelength. The function creates a line made up of SEND [filename] [filelength].
-- The function then calls startServer.
----------------------------------------------------------------------------------------------------------------------*/
void sendCommand(char *sbuf, int sd, struct sockaddr_in server,struct sockaddr_in client)
{
	char * filename;
	char line[MAXLEN];
	char length[MAXLEN];
	char s[5];
	char space[2];
	strcpy(s, "SEND");
	strcpy(space, " ");

	strtok(sbuf, "\n");

	memset(line, '\0', sizeof(line));
	filename = strtok (sbuf," ");
	if (filename != NULL)
	{
		filename = strtok (NULL, " ");
	}

	FILE *fp;
	if ((fp = fopen(filename, "rb")) == NULL)
	{
		if (!fp)perror("fopen");
		printf("Could not open file %s\n", filename);
	}
	else
	{
		fseek(fp, 0, SEEK_END);
		int fileLength = ftell(fp);
		sprintf(length, "%d", fileLength);

		fclose(fp);
		strcpy(line, s);
		strcat(line, space);
		strcat(line, filename);
		strcat(line, space);
		strcat(line, length);

		startServer(s, filename, fileLength, line, sd, server, client);
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: startServer
--
--	DATE:			October 1, 2018
--
-- REVISIONS: (Date and Description)
-- N/A
--
-- DESIGNER: Haley Booker
--
--	PROGRAMMERS:	Haley Booker
--
-- INTERFACE: void startServer(char *command, char *filename, int fileLength, char *line, int clientSD)
-- char *command: The command
-- char *filename: The name of the file
-- int fileLength: The length of the file
-- char *line: The command line "[COMMAND] [filename] [filelength]"
-- int clientSD: The socket descriptor of the client
--
-- RETURNS: void
--
-- NOTES:
-- Creates a server in the client application for the server program to listen to. Once created clientListen is
-- called.
----------------------------------------------------------------------------------------------------------------------*/
void startServer(char *command, char *filename, int fileLength, char *line, int clientSD, struct sockaddr_in server, struct sockaddr_in client)
{
	int	sd, port;
	struct	sockaddr_in clientServer;

	port = 7008;	// Get user specified port

	// Create a stream socket
    if ((sd = socket (AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror ("Can't create a socket");
		exit(1);
	}

    // Bind an address to the socket
	bzero((char *)&clientServer, sizeof(clientServer));
	clientServer.sin_family = AF_INET;
	clientServer.sin_port = htons(port);
	clientServer.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind (sd, (struct sockaddr *)&clientServer, sizeof(clientServer)) == -1)
	{
		perror ("Can't bind name to socket");
		exit(1);
	}
	clientListen(sd, command, filename, fileLength, line, clientSD, server,client,clientServer);
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: clientListen
--
--	DATE:			October 1, 2018
--
-- REVISIONS: (Date and Description)
-- N/A
--
-- DESIGNER: Haley Booker
--
--	PROGRAMMERS:	Haley Booker
--
-- INTERFACE: void clientListen (int sd, char *command, char *filename, int fileLength, char *line, int clientSD)
-- int	sd: The socket descriptor of the client servers
-- char *command: The command
-- int fileLength: The length of the file
-- char *line: The command line "[COMMAND] [filename] [filelength]"
-- int clientSD: The socket descriptor of the client
--
-- RETURNS: void
--
-- NOTES:
-- Listens for a connection from the server. Once the server connects, readServer is called.
----------------------------------------------------------------------------------------------------------------------*/
void clientListen (int	sd, char *command, char *filename, int fileLength, char *line, int clientSD, struct sockaddr_in server, struct sockaddr_in client, struct sockaddr_in clientServer)
{
	struct	sockaddr_in serverClient;
    socklen_t server_len;
	struct hostent	*hp;
    int port  = 7006;

    // Store server's information
	bzero((char *)&serverClient, sizeof(serverClient));
	serverClient.sin_family = AF_INET;
	serverClient.sin_port = htons(port);

	if ((hp = gethostbyname(host)) == NULL)
	{
		fprintf(stderr,"Can't get server's IP address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&serverClient.sin_addr, hp->h_length);
    server_len = sizeof(server);
    // Command send
    packetGen(line,fileLength,server,client);
    int ackWin[1] = {0};
    printf("\n %s \n",packet);
    if (sendto (clientSD, packet, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
    {
        perror("sendto failure");
        exit(1);
    }

    memset(packet,0,MAXLEN);
    readServer(sd, serverClient, command, filename, fileLength, clientServer);

    close(sd);
    return;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: readServer
--
--	DATE:			October 1, 2018
--
-- REVISIONS: (Date and Description)
-- N/A
--
-- DESIGNER: Haley Booker
--
--	PROGRAMMERS:	Haley Booker
--
-- INTERFACE: int readServer(int new_sd, struct sockaddr_in client, char *command, char *filename, int fileLength)
-- int new_sd: The newly opened socket
-- struct sockaddr_in client: The information on the server
-- char *command: The command SEND or START
-- char *filename: The name of the file
-- int fileLength: The length of the file
--
-- RETURNS: An integer for error checking
--
-- NOTES:
-- If the command is SEND the client will send the file to the server on the new socket.
-- If the command is START the client will wait for the server to send over the file. If the file doesn't
-- exist, it'll be created. If it does exist the content will be overwritten. It then closes the file and socket.
----------------------------------------------------------------------------------------------------------------------*/
int readServer(int new_sd, struct sockaddr_in client, char *command, char *filename, int fileLength, struct sockaddr_in clientServer)
{
	socklen_t client_len;

    client_len= sizeof(client);printf("\n%d",fileLength);

	while(TRUE)
	{
		if (strncmp("SEND", command, 4) == 0)
		{
			FILE *fp;

			if ((fp = fopen(filename, "rb")) == NULL)
			{
				if (!fp)perror("fopen");
				printf("Could not open file %s\n", filename);
			}
			else
			{
                char	*bp;
	            int	n, bytes_to_read;
                char	buf[MAXLEN];
				char line[fileLength];
                line[fileLength-1] = 0;
				//int newlines = countNewLine(fp);
				char window[MAXLEN][MAXLEN];
				int ackWin[MAXLEN];

				double  windowSize = ceil(fileLength/1024.0);
				printf("\n %f",windowSize);
				for(int index = 0;index < windowSize;index++)
				{
                    memset(line,0,fileLength);
					fread(line, 1024, 1, fp);
					packetGen(line,1024,client,clientServer);
					strcpy(window[index],packet);
					ackWin[index] = 0;
					printf("\n%s",window[index]);
                    seqNum++;
				}
				fclose(fp);
                fd_set readFDS;
                struct timeval timeOut;
                
                FD_SET(new_sd,&readFDS);
                int nextAck = 0;
                int selectVal;
                int index = 0;
                int slidingWindow = ceil(windowSize/2.0);
                int loop;
                while(index < windowSize)
                {
                    loop = 1;
				    for(; index < slidingWindow;index++)
				    {
                        if(ackWin[index] == 0)
                        {
                            if (sendto (new_sd, window[index], MAXLEN, 0,(struct sockaddr *)&client, client_len) == -1)
    				    	{
                                perror ("sendto error");
                                exit(1);
                            }
                        }
				    }
                    while(loop)
                    {
                        timeOut.tv_sec = 0;
                        timeOut.tv_usec = 2000;
                        FD_ZERO(&readFDS);
                        FD_SET(new_sd,&readFDS);
                        selectVal = select(new_sd+1,&readFDS,0,0,&timeOut);
                        printf("%d \n",selectVal);
                        perror("select() failure");
                        if(selectVal != 0)
                        {
                            printf("Select found\n");
                            if(FD_ISSET(new_sd,&readFDS))
                            {
                                bp = buf;
                                bytes_to_read = MAXLEN;
                                while ((n = recvfrom (new_sd, bp, MAXLEN, 0, (struct sockaddr *)&client, &client_len)) < bytes_to_read)
                                {
                                    bp += n;
                                    bytes_to_read -= n;
                                }
                                printf("%s \n",buf);
                                genPacketStruct(buf);
                                if(atoi(packetS->ackNum) == nextAck)
                                {
                                    nextAck++;
                                    slidingWindow++;
                                }
                                printf("Acked Data\n");
                                printf("%s \n",packetS->ackNum);
                                ackWin[atoi(packetS->ackNum)] = 1;
                            }
                        }
                        else
                        {
                            printf("Else break\n");
                            loop = 0;
                        }
                        printf("End of while 1 \n");
                        
                        
                    }
                    index = nextAck;
                }
                printf("Send FIN\n");
                seqNum = 0;
                packetGen("FIN", 0, client, clientServer);
                if (sendto (new_sd, packet, MAXLEN, 0,(struct sockaddr *)&client, client_len) == -1)
    			{
                   perror ("sendto error");
                   exit(1);
                }

			}
		}
        close(new_sd);
		return 0;
	}
}

void packetGen(char * line, int fileLength,struct sockaddr_in server,struct sockaddr_in client)
{
    memset(packet, 0 , MAXLEN);    
    char * temp = (char *) malloc(15);
    char space[2];//break point  character
    strcpy(space," ");
    sprintf (temp , "%d" ,seqNum);//seqNum
    strcpy(packet,temp);
    strcat(packet,space);
    sprintf(temp , "%d", ackNum);//ackNum
    strcat(packet,temp);
    strcat(packet,space);
    strcat(packet,inet_ntoa(server.sin_addr));
    strcat(packet,space);
    sprintf(temp,"%d",htons( server.sin_port));
    strcat(packet,temp);
    strcat(packet,space);
    strcat(packet,localIP);
    strcat(packet,space);
    sprintf(temp,"%d", htons(client.sin_port));
    strcat(packet,temp);
    strcat(packet,space);
    sprintf(temp,"%d", fileLength);
    strcat(packet,temp);
    strcat(packet,space);
    strcat(packet,line);
    printf("\nPacket Value: %s \n",packet);
}

void genPacketStruct(char *buffer)
{
    free(packetS);
    packetS = (struct packetStruct * )malloc(sizeof(struct packetStruct));
	strcpy(packetS->seqNum,strtok(buffer, " "));
	strcpy(packetS->ackNum,strtok(NULL," "));
	strcpy(packetS->dest,strtok(NULL," "));
	strcpy(packetS->destPrt,strtok(NULL," "));
	strcpy(packetS->src,strtok(NULL," "));
	strcpy(packetS->srcPrt,strtok(NULL," "));
	strcpy(packetS->dataLength,strtok(NULL," "));
	strcpy(packetS->data,strtok(NULL," "));
	char * temp = strtok(NULL," ");
	char space[2] = " ";
	while(temp != NULL)
	{
		strcat(packetS->data,space);
		strcat(packetS->data,temp);

		temp = strtok(NULL," ");
	}
}

int countNewLine(FILE * fp)
{
    int count = 0;
    char fileChar;
    while(fileChar != EOF)
    {
	fileChar = getc(fp);
	if(fileChar == '\n')
	{
	    count++;
	}
    }
    return count;

}
