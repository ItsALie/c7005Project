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
#include <sys/time.h>
#include "router_packet.h"

#define TRUE	1
#define MAXLEN			2000	//Buffer length
#define SERVER_PORT     7009	// Default port

struct packetStruct *packetS;
char packet[MAXLEN];
struct	sockaddr_in server;
int	sd;
int dropRate = 0;
int delay;

int readData();
void sendData();
void packetGen();
void genPacketStruct(char * buffer);

int main (int argc, char **argv)
{
	int port = SERVER_PORT;
	packetS = (struct packetStruct * )malloc(sizeof(struct packetStruct));
	switch(argc)
	{
		case 3:
			dropRate = atoi(argv[1]);	// Default port
			delay = atoi(argv[2]);
		break;
		default:
			fprintf(stderr, "Usage: %s dropRate\n", argv[0]);
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

    readData();
    close(sd);
	return(0);
}

int readData()
{
	char	*bp;
	int	n, bytes_to_read;
    char	buf[MAXLEN];
	struct	sockaddr_in client;
	socklen_t client_len;
	int dropValue;
	char window[MAXLEN][MAXLEN];
	double timeStamps[MAXLEN];
	double time_in_mill;
	struct timeval  tv;
	int index = 0;
	int startIndex = 0;

    client_len = sizeof(client);

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
		strcpy(window[index], buf);
		gettimeofday(&tv, NULL);
		time_in_mill = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ; // convert tv_sec & tv_usec to millisecond
		timeStamps[index] = time_in_mill;
 		index++;

		for (int i = startIndex; i < index; i++)
		{
			gettimeofday(&tv, NULL);
			time_in_mill = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ; // convert tv_sec & tv_usec to millisecond
			if (time_in_mill - timeStamps[i] > delay)
			{
				startIndex++;
				dropValue = (rand() % 100) + 1;

				if (dropValue > dropRate)
				{
					genPacketStruct(window[i]);
			        sendData();
				}
				else
				{
					printf("Dropped syn %s, ack %s for %s %s\n", packetS->seqNum, packetS->ackNum, packetS->dest, packetS->destPrt);
				}
			}
		}
    }
	return 1;
}

void sendData()
{
	int port;
	struct hostent	*hp;
	struct sockaddr_in client;
	char  *host;
    socklen_t client_len;

	host =	packetS->dest;	// Host name
	port =	atoi(packetS->destPrt);

    // Store server's information
	bzero((char *)&client, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(port);

	if ((hp = gethostbyname(host)) == NULL)
	{
		fprintf(stderr,"Can't get server's IP address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&client.sin_addr, hp->h_length);

    client_len= sizeof(client);
    packetGen();
    if (sendto (sd, packet, MAXLEN, 0, (struct sockaddr *)&client, client_len) == -1)
    {
        perror("sendto failure");
        exit(1);
    }
	printf("Sent syn %s, ack %s to %s %s\n", packetS->seqNum, packetS->ackNum, packetS->dest, packetS->destPrt);
}

void genPacketStruct(char *buffer)
{
	free(packetS);
	packetS = (struct packetStruct * )malloc(sizeof(struct packetStruct));

	strcpy(packetS->seqNum,strtok(buffer," "));
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

void packetGen()
{
	memset(packet, 0, MAXLEN);
    char space[2];//break point  character
    strcpy(space," ");

    strcpy(packet, packetS->seqNum);
    strcat(packet, space);
    strcat(packet, packetS->ackNum);
    strcat(packet, space);
    strcat(packet, packetS->dest);
    strcat(packet, space);
    strcat(packet, packetS->destPrt);
    strcat(packet, space);
    strcat(packet, packetS->src);
    strcat(packet, space);
    strcat(packet, packetS->srcPrt);
    strcat(packet, space);
    strcat(packet, packetS->dataLength);
    strcat(packet, space);
    strcat(packet, packetS->data);
}
