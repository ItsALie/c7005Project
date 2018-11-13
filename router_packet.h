#ifndef ROUTER_PACKET_
#define ROUTER_PACKET_
struct packetStruct
{
    char dataLength[5];
    char seqNum[5];
    char ackNum[5];
    char dest[16];
    char destPrt[5];
    char src[16];
	char srcPrt[5];
    char data[1024];
};
#endif
