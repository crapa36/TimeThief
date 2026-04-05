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
};

struct FExportedNavData
{
	FExportedNavMeta Meta;
	TArray<FExportedNavLink> Links;
};