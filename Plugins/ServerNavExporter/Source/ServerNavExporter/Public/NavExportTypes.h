#pragma once

#include "CoreMinimal.h"

struct FServerNavBinaryHeader
{
	uint32 Magic = 0x54414E53; // 'SNAV' Server Nav
	uint32 Version = 1;
	
	uint32 TileCount = 0;
	
	float OrigX = 0.f;
	float OrigY = 0.f;
	float OrigZ = 0.f;
	
	float TileWidth = 0.f;
	float TileHeight = 0.f;
	
	int32 MaxTiles = 0;
	int32 MaxPolys = 0;
	
	float AgentRadius = 0.f;
	float AgentHeight = 0.f;
	float AgentStepHeight = 0.f;
	
	float CellSize = 0.f;
	float CellHeight = 0.f;
	float TileSizeUU = 0.f;
};

struct FServerNavTileHeader
{
	uint32 TileRef = 0;
	uint32 TileDataSize = 0;
};

struct FExportedNavLink
{
	FString Name;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	bool bBidirectional = false;
	bool bSmartLinkIsRelevant = false;
	bool bSmartLinkEnabled = false;
	
	bool bStartProjected = false;
	bool bEndProjected = false;
	FVector ProjectedStart = FVector::ZeroVector;
	FVector ProjectedEnd = FVector::ZeroVector;
};

struct FExportedNavPoly
{
	int32 PolyId = 0;
	
	TArray<FVector> Vertices;
	
	TArray<int32> Neighbors;
};

struct FExportedNavTile
{
	int32 TileIndex = 0;
	
	FVector MinBound = FVector::ZeroVector;
	FVector MaxBound = FVector::ZeroVector;
	
	TArray<FExportedNavPoly> Polys;
};

struct FExportedNavMeta
{
	FString MapName;
	FString NavDataClassName;
	
	FString CoordinateSystem = TEXT("UE_Z_UP_CM");
	
	float AgentRadius = 0.f;
	float AgentHeight = 0.f;
	float AgentStepHeight = 0.f;
	float AgentMaxSlope = 0.f;
	
	float CellSize = 0.f;
	float CellHeight = 0.f;
	float TileSizeUU = 0.f;
	
	int32 TileCount = 0;
};

struct FExportedNavData
{
	FExportedNavMeta Meta;
	
	TArray<FExportedNavLink> Links;
	TArray<FExportedNavTile> Tiles;
};
