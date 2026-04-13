#pragma once
#include "Protocol/Framing/PacketHeader.h"

using TTPacketHeader = Protocol::Framing::PacketHeader;

struct TIMETHIEF_API FPacketHeader : public TTPacketHeader
{
	
	FPacketHeader() : TTPacketHeader{}
	{
		
	}
	
	FPacketHeader(uint16 InPacketSize, uint16 InPacketId) : TTPacketHeader{InPacketSize, InPacketId}
	{
		
	}
	
	friend FArchive& operator<<(FArchive& Ar, FPacketHeader& Header)
	{
		Ar << Header.packetSize;
		Ar << Header.messageId;
		
		return Ar;
	}
};

