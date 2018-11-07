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

#define SERVER_UDP_PORT 7005	// Default port
#define TRUE	1
#define MAXLEN			65000	//Buffer length

void serverListen (int	sd);
int readClient(int sd);
void startClient(char *line, struct sockaddr_in client);

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
	struct	sockaddr_in server;

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
	char length[MAXLEN];
    char	buf[MAXLEN];
	struct	sockaddr_in client;
	socklen_t client_len;

    char s[5];
	char g[4];
	char space[2];
	strcpy(s, "SEND");
	strcpy(g, "GET");
	strcpy(space, " ");
    client_len = sizeof(client);

	bp = buf;
	while(TRUE)
	{
		bytes_to_read = MAXLEN;

		while ((n = recvfrom (sd, bp, MAXLEN, 0, (struct sockaddr *)&client, &client_len)) < bytes_to_read)
		{
			bp += n;
			bytes_to_read -= n;
		}

		if (strncmp("SEND",buf, 4) == 0)
		{
			startClient(buf, client);
		}
		if (strncmp("GET",buf, 3) == 0)
		{
            char * filename;
            filename = strtok (buf," ");
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
                strcpy(line, g);
                strcat(line, space);
                strcat(line, filename);
                strcat(line, space);
                strcat(line, length);
                send(sd, line, MAXLEN, 0);
            }
		}
        if (strncmp("START",buf, 5) == 0)
        {
			startClient(buf, client);
        }
		if (strncmp("CLOSE",buf, 5) == 0)
		{
			return 0;
		}
		if (strncmp("EXIT",buf, 4) == 0)
		{
			return 1;
		}
	}
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
void startClient(char *line, struct sockaddr_in client)
{
	int n, bytes_to_read;
	int sd, port;
	struct hostent	*hp;
	struct sockaddr_in server;
	char  *host, *bp, buf[MAXLEN];
    socklen_t client_len;

	char * command;
	char * filename;
	char * fileLength;

	command = strtok (line," ");
	filename = strtok (NULL, " ");
	fileLength = strtok (NULL, " ");
	int length = atoi(fileLength);

	host =	inet_ntoa(client.sin_addr);	// Host name
	port =	7006;

    // Create a datagram socket
	if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror ("Can't create a socket\n");
		exit(1);
	}

    // Store server's information
	bzero((char *)&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if ((hp = gethostbyname(host)) == NULL)
	{
		fprintf(stderr,"Can't get server's IP address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);

	// Bind local address to the socket
	bzero((char *)&client, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(0);
	client.sin_addr.s_addr = htonl(INADDR_ANY);

    client_len= sizeof(client);

	if (bind(sd, (struct sockaddr *)&client, sizeof(client)) == -1)
	{
		perror ("Can't bind name to socket");
		exit(1);
	}

	if (strncmp("SEND",command, 4) == 0)
	{
		bytes_to_read = length - 1;
		bp = buf;

		while ((n = recvfrom (sd, bp, bytes_to_read, 0, (struct sockaddr *)&client, &client_len)) < bytes_to_read)
		{
			bp += n;
			bytes_to_read -= n;
		}
		FILE *fp = fopen(filename, "wb+");
		if (fp != NULL)
		{
			fputs(bp, fp);
			fclose(fp);
		}
	}
	if (strncmp("START",command, 3) == 0)
	{
        FILE *fp;

        if ((fp = fopen(filename, "rb")) == NULL)
        {
            if (!fp)perror("fopen");
            printf("Could not open file %s\n", filename);
        }
        else
        {
            line  = malloc(length + 1);
            fread(line, length, 1, fp);
            fclose(fp);
            line[length] = 0;

            sendto (sd, line, length, 0,(struct sockaddr *)&client, client_len);
        }
	}
	close (sd);
}
