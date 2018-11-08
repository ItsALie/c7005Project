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

#define SERVER_UDP_PORT		7005	// Default port
#define MAXLEN			65000  	// Buffer length
#define TRUE	1
void clientConnection(int sd, struct sockaddr_in server);
void sendCommand(char *sbuf, int sd, struct sockaddr_in server);
void getCommand(char *sbuf, int sd, struct sockaddr_in server);
void startServer(char *command, char *filename, int fileLength, char *line, int clientSD, struct sockaddr_in server);
void clientListen (int	sd, char *command, char *filename, int fileLength, char *line, int clientSD, struct sockaddr_in server);
int readServer(int new_sd, struct sockaddr_in client, char *command, char *filename, int fileLength);
char  *host;

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

	struct	sockaddr_in client;

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
	clientConnection(sd, server);
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
void clientConnection(int sd, struct sockaddr_in server)
{
	int n, bytes_to_read;
	char sbuf[MAXLEN], *bp, rbuf[MAXLEN];
	char s[5];
	char g[4];
	char space[2];
	strcpy(s, "SEND");
	strcpy(g, "GET");
	strcpy(space, " ");
    socklen_t server_len;

    server_len = sizeof(server);

	while (TRUE)
	{
		printf("Transmit:\n");
		//gets(sbuf); // get user's text
		fgets (sbuf, MAXLEN, stdin);

		// Transmit data through the socket
		if (strncmp("SEND",sbuf, 4) == 0)
		{
			sendCommand(sbuf, sd, server);
		}
		if (strncmp("GET",sbuf, 3) == 0)
		{
            strtok(sbuf, "\n");
            if (sendto (sd, sbuf, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
            {
                perror("sendto failure");
                exit(1);
            }

			n = 0;
            bp = rbuf;
			bytes_to_read = MAXLEN;

			while((n = recvfrom (sd, bp, MAXLEN, 0, (struct sockaddr *)&server, &server_len)) < MAXLEN)
			{
				bp += n;
				bytes_to_read -= n;
			}
			getCommand(rbuf, sd, server);
		}
		if (strncmp("CLOSE",sbuf, 5) == 0)
		{
            if (sendto (sd, sbuf, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
            {
                perror("sendto failure");
                exit(1);
            }
			close (sd);
			return;
		}
		if (strncmp("EXIT",sbuf, 4) == 0)
		{
            if (sendto (sd, sbuf, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
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
void sendCommand(char *sbuf, int sd, struct sockaddr_in server)
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

		startServer(s, filename, fileLength, line, sd, server);
	}
}

/*------------------------------------------------------------------------------------------------------------------
-- FUNCTION: getCommand
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
-- INTERFACE: void getCommand(char *sbuf, int sd)
-- char *sbuf: The command line "GET [filename] [filelength]"
-- int sd: The socket descriptor
--
-- RETURNS: void
--
-- NOTES:
-- Removes the added newline character to the user input string. It takes the line and breaks
-- it into the filename and filelength. The function creates a line made up of START [filename] [filelength].
-- The function then calls startServer.
----------------------------------------------------------------------------------------------------------------------*/
void getCommand(char *sbuf, int sd, struct sockaddr_in server)
{
	char * filename;
	char line[MAXLEN];
	char start[6];
	char space[2];
	strcpy(start, "START");
	strcpy(space, " ");

	memset(line, '\0', sizeof(line));

	char * length;

	filename = strtok (sbuf," ");
	filename = strtok (NULL, " ");
	length = strtok (NULL, " ");
	int fileLength = atoi(length);

    strcpy(line, start);
    strcat(line, space);
    strcat(line, filename);
    strcat(line, space);
    strcat(line, length);

    startServer(start, filename, fileLength, line, sd, server);
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
void startServer(char *command, char *filename, int fileLength, char *line, int clientSD, struct sockaddr_in server)
{
	int	sd, port;
	struct	sockaddr_in clientServer;

	port = 7006;	// Get user specified port

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
	clientListen(sd, command, filename, fileLength, line, clientSD, server);
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
void clientListen (int	sd, char *command, char *filename, int fileLength, char *line, int clientSD, struct sockaddr_in server)
{
	struct	sockaddr_in serverClient;
    socklen_t server_len;
	struct hostent	*hp;
    
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

    if (sendto (clientSD, line, MAXLEN, 0, (struct sockaddr *)&server, server_len) == -1)
    {
        perror("sendto failure");
        exit(1);
    }

    readServer(sd, serverClient, command, filename, fileLength);

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
int readServer(int new_sd, struct sockaddr_in client, char *command, char *filename, int fileLength)
{
    int n, bytes_to_read;
	char *bp, buf[MAXLEN];
	socklen_t client_len;

    client_len= sizeof(client);

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
				char * line  = malloc(fileLength + 1);
				fread(line, fileLength, 1, fp);
				fclose(fp);
				line[fileLength] = 0;
                if (sendto (new_sd, line, fileLength, 0,(struct sockaddr *)&client, client_len) == -1)
                {
                    perror ("sendto error");
                    exit(1);
                }
			}
		}
		if (strncmp("START", command, 3) == 0)
		{
            bytes_to_read = fileLength - 1;
            bp = buf;

            while ((n = recvfrom (new_sd, bp, bytes_to_read, 0, (struct sockaddr *)&client, &client_len)) < bytes_to_read)
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
        close(new_sd);
		return 0;
	}
}
