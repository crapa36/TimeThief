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
};

struct FExportedNavMeta
{
	FString MapName;
	FString NavDataClassName;
};

struct FExportedNavData
{
	FExportedNavMeta Meta;
	TArray<FExportedNavLink> Links;
};