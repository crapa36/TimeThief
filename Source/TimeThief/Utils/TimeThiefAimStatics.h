#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TimeThiefAimStatics.generated.h"

class APawn;
class UWorld;

struct TIMETHIEF_API FTimeThiefAimHelperState
{
	FVector TargetLocation = FVector::ZeroVector;
	bool bHasTargetLocation = false;
	bool bWasUsingCloseHitSkip = false;
};

UCLASS()
class TIMETHIEF_API UTimeThiefAimStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr float AimHelperCloseHitSkipDistance = 250.0f;
	static constexpr float AimHelperCloseHitSkipHysteresis = 25.0f;
	static constexpr int32 AimHelperMaxCloseHitSkipCount = 4;

	static FVector NormalizeAimDirection(
		const FVector& Direction,
		const FVector& FallbackDirection = FVector::ForwardVector);

	static FVector ResolveAimTargetLocation(
		const FVector& Origin,
		const FVector& Direction,
		float Range,
		const FVector& FallbackDirection = FVector::ForwardVector);

	static FVector ResolveAimDirectionToTarget(
		const FVector& StartLocation,
		const FVector& TargetLocation,
		const FVector& FallbackDirection = FVector::ForwardVector);

	static FRotator ResolveAimRotationFromDirection(
		const FVector& Direction,
		const FRotator& FallbackRotation = FRotator::ZeroRotator);

	static void ResolveRelativeAimPitchYaw(
		const FTransform& BasisTransform,
		const FVector& AimDirectionWorld,
		float& OutPitch,
		float& OutYaw,
		const FVector& FallbackDirection = FVector::ForwardVector);

	static FRotator BuildAimRotation(float Pitch, float Yaw, float Roll = 0.0f);

	static FVector ResolveAimDirectionFromRotation(const FRotator& AimRotation);

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
		bool bTraceComplex = false, 
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
		bool bTraceComplex = false,
		bool bReturnPhysicalMaterial = false);

	static void ResetAimHelperState(
		FTimeThiefAimHelperState& InOutAimHelperState,
		const FVector& TargetLocation,
		bool bUsingCloseHitSkip);

	static FVector UpdateAimHelperTargetFromView(
		FTimeThiefAimHelperState& InOutAimHelperState,
		UWorld* World,
		const FVector& ViewLocation,
		const FVector& ViewDirection,
		float Range,
		const TArray<AActor*>& ActorsToIgnore,
		const FVector& DistanceOrigin,
		ECollisionChannel TraceChannel = ECC_Visibility,
		bool bTraceComplex = false,
		bool bReturnPhysicalMaterial = false);

	static bool TraceLine(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const TArray<AActor*>& ActorsToIgnore,
		FHitResult& OutHitResult,
		ECollisionChannel TraceChannel = ECC_Visibility,
		bool bTraceComplex = false,
		bool bReturnPhysicalMaterial = false);
};
