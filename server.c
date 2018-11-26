/*---------------------------------------------------------------------------------------
--	SOURCE FILE:		server.c - A simple file transfer system
--
--	PROGRAM:		server.exe
--
--	FUNCTIONS:
--		int main (int argc, char **argv);
--		void serverListen (int	sd);
--		int readClient(int sd, struct sockaddr_in client);
--		void startClient(char *line, struct sockaddr_in client);
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
--	The server program of a file transfer system. The server will start on
-- 	port 7005 and wait for a request from a client. It accepts the request
-- 	and then waits for input from the client.
--	If it recieves a SEND [filename] [filelength], it will start a socket to
-- 	connect to the client on port 7006 and then wait to recieve the file. If
--	the file doesn't exist, it'll be created. If the file does exist, it will
--	be overwritten.
--	If it recieves a GET [filename], it will search for the filename. If it
--	finds the file it will send back GET [filename] [filelength]. The server
--	then waits for the client to send START [filename] [filelength]. The
--	server then creates a socket on port 7006 and connects to the client.
-- 	Once connected the server will send over the file.
--	After SEND/GET command is completed the 7006 socket will close and wait
-- 	for the next command from the client.
--	If the server recieves CLOSE, it will end the connection with the client.
-- 	If the server recieves EXIT, it will end the connection with the client and
--	close the server.
---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include "router_packet.h"

#define SERVER_UDP_PORT 7005	// Default port
#define TRUE	1
#define MAXLEN			2000	//Buffer length

struct packetStruct *packetS;
char packet[MAXLEN];
struct sockaddr_in serverClient;
struct	sockaddr_in server;
int datasd;
int seqNum;
int ackNum;
char localIP[16];
char * localIPBuff;

void serverListen (int	sd);
int readClient(int sd);
void startClient(struct sockaddr_in client);
void packetGen(char * line, int fileLength);
void genPacketStruct(char * buffer);
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
-- The start point of the program. The function creates the server and calls serverListen.
----------------------------------------------------------------------------------------------------------------------*/
int main (int argc, char **argv)
{
	int	sd, port;
	char hostbuffer[256];
	struct hostent *host_entry;
	packetS = (struct packetStruct * )malloc(sizeof(struct packetStruct));
	switch(argc)
	{
		case 1:
			port = SERVER_UDP_PORT;	// Default port
		break;
		case 2:
			port = atoi(argv[1]);	//User specified port
		break;
		default:
			fprintf(stderr, "Usage: %s [port]\n", argv[0]);
			exit(1);
   	}
    seqNum = 0;
    ackNum = 0;

	gethostname(hostbuffer, sizeof(hostbuffer));
	host_entry = gethostbyname(hostbuffer);
	localIPBuff = inet_ntoa(*((struct in_addr*)
    host_entry->h_addr_list[0]));
	strcpy(localIP,localIPBuff);

	// Create a datagram socket
	if ((sd = socket (AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror ("Can't create a socket");
		exit(1);
	}

	// Bind an address to the socket
	bzero((char *)&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind (sd, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		perror ("Can't bind name to socket");
		exit(1);
	}

	// Create a datagram socket
	if ((datasd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror ("Can't create a socket\n");
		exit(1);
	}

	// Bind local address to the socket
	bzero((char *)&serverClient, sizeof(serverClient));
	serverClient.sin_family = AF_INET;
	serverClient.sin_port = htons(7006);
	serverClient.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(datasd, (struct sockaddr *)&serverClient, sizeof(serverClient)) == -1)
	{
		perror ("Can't bind name to socket");
		exit(1);
	}

	serverListen(sd);
	return(0);
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: serverListen
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
-- INTERFACE: void serverListen (int sd)
-- int sd: The socket descriptor of the server
--
-- RETURNS: void
--
-- NOTES:
-- The server listens for connections from clients. Once a connection is received, readClient is called.
-- If readClient returns 1, the server will exit.
----------------------------------------------------------------------------------------------------------------------*/
void serverListen (int	sd)
{
	while (TRUE)
	{
		int retnum = readClient(sd);

		//EXIT was sent
		if (retnum == 1)
		{
			close(sd);
			close (datasd);
			return;
		}
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: readClient
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
-- INTERFACE: int readClient(int sd, struct sockaddr_in client)
-- int sd: The socket descriptor of the client
-- struct sockaddr_in client: The information of the client
--
-- RETURNS: An integer. 0 means the client is exiting. 1 means the server will close.
--
-- NOTES:
-- Waits for the client to send a line.
-- If the client sends SEND [filename] [filelength], startClient is called.
-- If the client sends GET [filename], it will search for the filename. If it finds the file it will send back
-- GET [filename] [filelength].
-- If the client sends START [filename] [filelength], startClient is called.
-- If the client sends CLOSE, the program closes the socket and returns 0.
-- If the client sends EXIT, the program closes the socket and returns 1.
----------------------------------------------------------------------------------------------------------------------*/
int readClient(int sd)
{
	char	*bp;
	int	n, bytes_to_read;
	char line[MAXLEN];
	memset(line, '\0', sizeof(line));
    char	buf[MAXLEN];
	struct	sockaddr_in client;
	socklen_t client_len;

    char s[5];
	char space[2];
	char syn[4];
	char synack[7];
	char ack[4];
	strcpy(syn, "SYN");
	strcpy(synack, "SYNACK");
	strcpy(ack, "ACK");
	strcpy(s, "SEND");
	strcpy(space, " ");
    client_len = sizeof(client);

    //Three way handshake
	bp = buf;
    bytes_to_read = MAXLEN;

    //SYN
    while ((n = recvfrom (sd, bp, MAXLEN, 0, (struct sockaddr *)&client, &client_len)) < bytes_to_read)
    {
        bp += n;
        bytes_to_read -= n;
    }
	printf("%s\n", buf);
    genPacketStruct(buf);
    if (strncmp(syn,packetS->data, 3) == 0)
    {
		//SYNACK
	    packetGen(synack, 0);
	    if (sendto (sd, packet, MAXLEN, 0, (struct sockaddr *)&client, client_len) == -1)
	    {
	        perror("sendto failure");
	        exit(1);
	    }
	}
    //ACK
	bp = buf;
    bytes_to_read = MAXLEN;
    while ((n = recvfrom (sd, bp, MAXLEN, 0, (struct sockaddr *)&client, &client_len)) < bytes_to_read)
    {
        bp += n;
        bytes_to_read -= n;
    }
	printf("ACK: %s\n", buf);
    while(TRUE)
    {
		memset(buf, 0, MAXLEN);
        bp = buf;
        bytes_to_read = MAXLEN;

        while ((n = recvfrom (sd, bp, MAXLEN, 0, (struct sockaddr *)&client, &client_len)) < bytes_to_read)
        {
            bp += n;
            bytes_to_read -= n;
        }
		printf("buffer%s\n", buf);
        genPacketStruct(buf);
        printf("Packet Data:%s\n",packetS->data);
        if (strncmp("SEND",packetS->data, 4) == 0)
        {
			printf("In SEND\n");
			startClient(client);
        }
        else if (strncmp("CLOSE",packetS->data, 5) == 0)
        {
            return 0;
        }
        else if (strncmp("EXIT",packetS->data, 4) == 0)
        {
			free(packetS);
            return 1;
        }
    }
	return 1;
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: startClient
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
-- INTERFACE: void startClient(char *line, struct sockaddr_in client)
-- char *line: The line containing [COMMAND] [filename] [filelength]
-- struct sockaddr_in client: The information of the client
--
-- RETURNS: void
--
-- NOTES:
-- Creates a socket and attempts to connect to the client.
-- If the command is SEND, it receives the file. If the file doesn't exist, it'll be created.
-- If the file does exist, it will be overwritten.
-- If the command is START, the server will send the file to the client.
----------------------------------------------------------------------------------------------------------------------*/
void startClient(struct sockaddr_in client)
{
	int n, bytes_to_read;
	char *bp, buf[MAXLEN];
    socklen_t client_len;
	char window[MAXLEN][MAXLEN];
	char ack[4];
	strcpy(ack, "ACK");
	int maxIndex = 0;

	char filename[MAXLEN];
	strcpy(filename, strtok (packetS->data," "));
	strcpy(filename, strtok (NULL, " "));

    client_len= sizeof(client);

	if (strncmp("SEND",packetS->data, 4) == 0)
	{
		while (TRUE)
		{
			bytes_to_read = MAXLEN;
			memset(buf, 0, MAXLEN);
			bp = buf;
			while ((n = recvfrom (datasd, bp, bytes_to_read, 0, (struct sockaddr *)&client, &client_len)) < bytes_to_read)
			{
				bp += n;
				bytes_to_read -= n;
			}
			printf("%s\n", buf);
			genPacketStruct(buf);
			//FIN sent
			if (strncmp("FIN",packetS->data, 3) == 0)
			{
				printf("FIN sent.\n");
				printf("%s\n", filename);
				FILE *fp = fopen(filename, "wb+");
				if (fp != NULL)
				{
					for (int i = 0; i <= maxIndex; i++)
					{
						fputs(window[i], fp);
					}
					fclose(fp);
				}
				return;
			}
			printf("%s\n", packetS->seqNum);
			ackNum = atoi(packetS->seqNum);
			packetGen(ack, 0);
		    if (sendto (datasd, packet, MAXLEN, 0, (struct sockaddr *)&client, client_len) == -1)
		    {
		        perror("sendto failure");
		        exit(1);
		    }
			strcpy(window[ackNum], packetS->data);
			if (ackNum > maxIndex)
			{
				maxIndex = ackNum;
			}
		}
	}
}

void genPacketStruct(char *buffer)
{
	free(packetS);
	packetS = (struct packetStruct * )malloc(sizeof(struct packetStruct));

	strcpy(packetS->seqNum,strtok(buffer," "));
	strcpy(packetS->ackNum,strtok(NULL," "));
	strcpy(packetS->src,strtok(NULL," "));
	strcpy(packetS->srcPrt,strtok(NULL," "));
	strcpy(packetS->dest,strtok(NULL," "));
	strcpy(packetS->destPrt,strtok(NULL," "));
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
	printf("SeqNum: %s\n", packetS->seqNum);
}

void packetGen(char * line, int fileLength)
{
	memset(packet, 0, MAXLEN);
    char * temp = (char *) malloc(15);
    char space[2];//break point  character
    strcpy(space," ");
    sprintf (temp , "%d" ,seqNum);//seqNum
    strcpy(packet,temp);
    strcat(packet,space);
    sprintf(temp , "%d", ackNum);//ackNum
    strcat(packet,temp);
    strcat(packet,space);
    strcat(packet,packetS->dest);
    strcat(packet,space);
    strcat(packet,packetS->destPrt);
    strcat(packet,space);
    strcat(packet,localIP);
    strcat(packet,space);
    strcat(packet,packetS->srcPrt);
    strcat(packet,space);
    sprintf(temp,"%d", fileLength);
    strcat(packet,temp);
    strcat(packet,space);
    strcat(packet,line);
	printf("%s\n", packet);
	free(temp);
}
