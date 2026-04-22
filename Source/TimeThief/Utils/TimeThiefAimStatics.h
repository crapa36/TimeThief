#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TimeThiefAimStatics.generated.h"

class APawn;
class UWorld;

UCLASS()
class TIMETHIEF_API UTimeThiefAimStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "TimeThief|Aim")
	static bool ResolveAimView(const APawn* Pawn, FVector& OutViewLocation, FVector& OutViewDirection);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Aim")
	static bool TraceAimHit(
		const APawn* Pawn,
		float Range,
		const TArray<AActor*>& ActorsToIgnore,
		FHitResult& OutHitResult,
		FVector& OutTraceEnd,
		ECollisionChannel TraceChannel = ECC_Visibility,
		bool bTraceComplex = true,
		bool bReturnPhysicalMaterial = false);

	static bool TraceFromView(
		UWorld* World,
		const FVector& ViewLocation,
		const FVector& ViewDirection,
		float Range,
		const TArray<AActor*>& ActorsToIgnore,
		FHitResult& OutHitResult,
		FVector& OutTraceEnd,
		ECollisionChannel TraceChannel = ECC_Visibility,
		bool bTraceComplex = true,
		bool bReturnPhysicalMaterial = false);

	static bool TraceLine(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const TArray<AActor*>& ActorsToIgnore,
		FHitResult& OutHitResult,
		ECollisionChannel TraceChannel = ECC_Visibility,
		bool bTraceComplex = true,
		bool bReturnPhysicalMaterial = false);
};

