#pragma once

#include "CoreMinimal.h"

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
