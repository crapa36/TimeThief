#pragma once

#include "CoreMinimal.h"
#include "Detour/DetourNavMesh.h"

constexpr uint32 SERVER_NAV_MAGIC = 0x4D56414E; // 'NAVM'
constexpr uint32 SERVER_NAV_VERSION = 1;

struct FServerNavBinaryHeader
{
	uint32 Magic = SERVER_NAV_MAGIC;
	uint32 Version = SERVER_NAV_VERSION;

	float Orig[3] = {};
	float TileWidth = 0.0f;
	float TileHeight = 0.0f;

	int32 MaxTiles = 0;
	int32 MaxPolys = 0;

	int32 TileCount = 0;

	// 검증용
	uint32 DtTileRefSize = sizeof(dtTileRef);
	uint32 DtNavMeshParamsSize = sizeof(dtNavMeshParams);
};

struct FServerNavTileHeader
{
	dtTileRef TileRef = 0;
	uint32 TileDataSize = 0;
};