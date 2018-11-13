#ifndef ROUTER_PACKET_
#define ROUTER_PACKET_
	struct packet
	{
	    
            int dataLength;
            int seqNum;
            int ackNum;
            char dest[16];
            int destPrt;
            char src[16];
            int srcPrt;
	    char data[1024];
	};
#endif
