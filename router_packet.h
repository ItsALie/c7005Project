#ifndef ROUTER_PACKET_
#define ROUTER_PACKET_
/*------------------------------------------------------------------------------------------------------------------
-- PROTORYPE: struct packetStruct
--
--	DATE:			December 1, 2018
--
-- REVISIONS: (Date and Description)
-- N/A
--
-- DESIGNER: Matthew Baldock
--
--	PROGRAMMERS:	Matthew Baldock
--
--
--
-- NOTES:
-- Header file definition 
----------------------------------------------------------------------------------------------------------------------*/
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
