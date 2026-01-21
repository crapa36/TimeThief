#pragma once

struct TempHeader
// TODO: github submodule로 교체 후 해당 repo로 부터 공유 받아 동기화 하도록
//		 그 전까지는 임시로 PacketHandler 등에서 사용하게 될 구조체
{
	uint16 size;
	uint16 id;
};

struct TIMETHIEF_API FTempHeader
{
	FTempHeader() : PacketSize(0), PacketId(0)
	{
		
	}
	
	FTempHeader(uint16 InPacketSize, uint16 InPacketId) : PacketSize(InPacketSize), PacketId(InPacketId)
	{
		
	}
	
	friend FArchive& operator<<(FArchive& Ar, FTempHeader& Header)
	{
		Ar << Header.PacketSize;
		Ar << Header.PacketId;
		
		return Ar;
	}
	
	uint16 PacketSize;
	uint16 PacketId;
};

