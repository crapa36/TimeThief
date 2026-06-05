#include "Utils/TimeThiefAimStatics.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

static FCollisionQueryParams BuildTraceParams(const TArray<AActor*>& ActorsToIgnore, bool bTraceComplex, bool bReturnPhysicalMaterial)
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefAimTrace), bTraceComplex);
	QueryParams.bReturnPhysicalMaterial = bReturnPhysicalMaterial;
	if (ActorsToIgnore.Num() > 0)
	{
		QueryParams.AddIgnoredActors(ActorsToIgnore);
	}
	return QueryParams;
}

static bool IsWithinAimHelperCloseHitDistance(const FVector& DistanceOrigin, const FVector& HitLocation, bool bWasUsingCloseHitSkip)
{
	const float CloseHitDistance = bWasUsingCloseHitSkip
		? UTimeThiefAimStatics::AimHelperCloseHitSkipDistance + UTimeThiefAimStatics::AimHelperCloseHitSkipHysteresis
		: UTimeThiefAimStatics::AimHelperCloseHitSkipDistance;
	return FVector::DistSquared(DistanceOrigin, HitLocation) < FMath::Square(CloseHitDistance);
}

static bool TraceViewTarget(
	UWorld* World,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	float Range,
	const FCollisionQueryParams& QueryParams,
	FHitResult& OutHitResult,
	FVector& OutTargetLocation,
	ECollisionChannel TraceChannel)
{
	OutHitResult = FHitResult();
	OutTargetLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(ViewLocation, ViewDirection, Range);
	if (!World)
	{
		return false;
	}

	const bool bHit = World->LineTraceSingleByChannel(OutHitResult, ViewLocation, OutTargetLocation, TraceChannel, QueryParams);
	if (bHit && OutHitResult.bBlockingHit)
	{
		OutTargetLocation = OutHitResult.ImpactPoint;
	}
	return bHit;
}

static FVector ResolveAimHelperRawTargetFromView(
	UWorld* World,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	float Range,
	const TArray<AActor*>& ActorsToIgnore,
	const FVector& DistanceOrigin,
	bool bWasUsingCloseHitSkip,
	bool& bOutUsingCloseHitSkip,
	ECollisionChannel TraceChannel,
	bool bTraceComplex,
	bool bReturnPhysicalMaterial)
{
	bOutUsingCloseHitSkip = false;

	FCollisionQueryParams QueryParams = BuildTraceParams(ActorsToIgnore, bTraceComplex, bReturnPhysicalMaterial);
	FHitResult Hit;
	FVector TargetLocation = FVector::ZeroVector;
	if (!TraceViewTarget(World, ViewLocation, ViewDirection, Range, QueryParams, Hit, TargetLocation, TraceChannel) || !Hit.bBlockingHit)
	{
		return TargetLocation;
	}

	if (!IsWithinAimHelperCloseHitDistance(DistanceOrigin, Hit.ImpactPoint, bWasUsingCloseHitSkip))
	{
		return Hit.ImpactPoint;
	}

	UPrimitiveComponent* HitComponent = Hit.GetComponent();
	if (!HitComponent)
	{
		return Hit.ImpactPoint;
	}

	bOutUsingCloseHitSkip = true;
	QueryParams.AddIgnoredComponent(HitComponent);

	for (int32 SkipCount = 0; SkipCount < UTimeThiefAimStatics::AimHelperMaxCloseHitSkipCount; ++SkipCount)
	{
		if (!TraceViewTarget(World, ViewLocation, ViewDirection, Range, QueryParams, Hit, TargetLocation, TraceChannel) || !Hit.bBlockingHit)
		{
			return TargetLocation;
		}

		if (!IsWithinAimHelperCloseHitDistance(DistanceOrigin, Hit.ImpactPoint, bWasUsingCloseHitSkip))
		{
			return Hit.ImpactPoint;
		}

		HitComponent = Hit.GetComponent();
		if (!HitComponent)
		{
			return TargetLocation;
		}

		QueryParams.AddIgnoredComponent(HitComponent);
	}

	return TargetLocation;
}

FVector UTimeThiefAimStatics::NormalizeAimDirection(const FVector& Direction, const FVector& FallbackDirection)
{
	const FVector SafeDirection = Direction.GetSafeNormal();
	if (!SafeDirection.IsNearlyZero())
	{
		return SafeDirection;
	}

	const FVector SafeFallback = FallbackDirection.GetSafeNormal();
	if (!SafeFallback.IsNearlyZero())
	{
		return SafeFallback;
	}

	return FVector::ForwardVector;
}

FVector UTimeThiefAimStatics::ResolveAimTargetLocation(
	const FVector& Origin,
	const FVector& Direction,
	float Range,
	const FVector& FallbackDirection)
{
	const FVector SafeDirection = NormalizeAimDirection(Direction, FallbackDirection);
	return Origin + (SafeDirection * Range);
}

FVector UTimeThiefAimStatics::ResolveAimDirectionToTarget(
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const FVector& FallbackDirection)
{
	return NormalizeAimDirection(TargetLocation - StartLocation, FallbackDirection);
}

FRotator UTimeThiefAimStatics::ResolveAimRotationFromDirection(
	const FVector& Direction,
	const FRotator& FallbackRotation)
{
	const FVector SafeDirection = Direction.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return FallbackRotation;
	}

	return SafeDirection.Rotation();
}

void UTimeThiefAimStatics::ResolveRelativeAimPitchYaw(
	const FTransform& BasisTransform,
	const FVector& AimDirectionWorld,
	float& OutPitch,
	float& OutYaw,
	const FVector& FallbackDirection)
{
	const FVector WorldDirection = NormalizeAimDirection(AimDirectionWorld, FallbackDirection);
	const FVector LocalDirection = NormalizeAimDirection(
		BasisTransform.InverseTransformVectorNoScale(WorldDirection),
		FVector::ForwardVector);

	OutYaw = FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Y, LocalDirection.X));
	const float HorizontalLength = FVector2D(LocalDirection.X, LocalDirection.Y).Size();
	OutPitch = FMath::RadiansToDegrees(FMath::Atan2(LocalDirection.Z, HorizontalLength));
}

FRotator UTimeThiefAimStatics::BuildAimRotation(float Pitch, float Yaw, float Roll)
{
	return FRotator(Pitch, Yaw, Roll);
}

FVector UTimeThiefAimStatics::ResolveAimDirectionFromRotation(const FRotator& AimRotation)
{
	return NormalizeAimDirection(AimRotation.Vector(), FVector::ForwardVector);
}

bool UTimeThiefAimStatics::ResolveAimView(const APawn* Pawn, FVector& OutViewLocation, FVector& OutViewDirection)
{
	if (!Pawn)
	{
		return false;
	}

	if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		FRotator CameraRotation;
		PC->GetPlayerViewPoint(OutViewLocation, CameraRotation);
		OutViewDirection = ResolveAimDirectionFromRotation(CameraRotation);
		return true;
	}

	OutViewLocation = Pawn->GetPawnViewLocation();
	OutViewDirection = ResolveAimDirectionFromRotation(Pawn->GetBaseAimRotation());
	return true;
}

bool UTimeThiefAimStatics::TraceAimHit(
	const APawn* Pawn,
	float Range,
	const TArray<AActor*>& ActorsToIgnore,
	FHitResult& OutHitResult,
	FVector& OutTraceEnd,
	ECollisionChannel TraceChannel,
	bool bTraceComplex,
	bool bReturnPhysicalMaterial)
{
	if (!Pawn)
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	if (!ResolveAimView(Pawn, ViewLocation, ViewDirection))
	{
		return false;
	}

	TArray<AActor*> ActualActorsToIgnore;
	ActualActorsToIgnore.Reserve(ActorsToIgnore.Num() + 1);
	ActualActorsToIgnore.Append(ActorsToIgnore);
	ActualActorsToIgnore.AddUnique(const_cast<APawn*>(Pawn));

	return TraceFromView(Pawn->GetWorld(), ViewLocation, ViewDirection, Range, ActualActorsToIgnore, OutHitResult, OutTraceEnd, TraceChannel, bTraceComplex, bReturnPhysicalMaterial);
}

bool UTimeThiefAimStatics::TraceFromView(
	UWorld* World,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	float Range,
	const TArray<AActor*>& ActorsToIgnore,
	FHitResult& OutHitResult,
	FVector& OutTraceEnd,
	ECollisionChannel TraceChannel,
	bool bTraceComplex,
	bool bReturnPhysicalMaterial)
{
	const FCollisionQueryParams QueryParams = BuildTraceParams(ActorsToIgnore, bTraceComplex, bReturnPhysicalMaterial);
	return TraceViewTarget(World, ViewLocation, ViewDirection, Range, QueryParams, OutHitResult, OutTraceEnd, TraceChannel);
}

bool UTimeThiefAimStatics::TraceFromViewWithParams(
	UWorld* World,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	float Range,
	const FCollisionQueryParams& QueryParams,
	FHitResult& OutHitResult,
	FVector& OutTraceEnd,
	ECollisionChannel TraceChannel)
{
	return TraceViewTarget(World, ViewLocation, ViewDirection, Range, QueryParams, OutHitResult, OutTraceEnd, TraceChannel);
}

void UTimeThiefAimStatics::ResetAimHelperState(
	FTimeThiefAimHelperState& InOutAimHelperState,
	const FVector& TargetLocation,
	bool bUsingCloseHitSkip)
{
	InOutAimHelperState.TargetLocation = TargetLocation;
	InOutAimHelperState.bHasTargetLocation = true;
	InOutAimHelperState.bWasUsingCloseHitSkip = bUsingCloseHitSkip;
}

FVector UTimeThiefAimStatics::UpdateAimHelperTargetFromView(
	FTimeThiefAimHelperState& InOutAimHelperState,
	UWorld* World,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	float Range,
	const TArray<AActor*>& ActorsToIgnore,
	const FVector& DistanceOrigin,
	ECollisionChannel TraceChannel,
	bool bTraceComplex,
	bool bReturnPhysicalMaterial)
{
	bool bUsingCloseHitSkip = false;
	const FVector RawTargetLocation = ResolveAimHelperRawTargetFromView(
		World,
		ViewLocation,
		ViewDirection,
		Range,
		ActorsToIgnore,
		DistanceOrigin,
		InOutAimHelperState.bWasUsingCloseHitSkip,
		bUsingCloseHitSkip,
		TraceChannel,
		bTraceComplex,
		bReturnPhysicalMaterial);

	if (!InOutAimHelperState.bHasTargetLocation)
	{
		ResetAimHelperState(InOutAimHelperState, RawTargetLocation, bUsingCloseHitSkip);
		return RawTargetLocation;
	}

	InOutAimHelperState.TargetLocation = RawTargetLocation;
	InOutAimHelperState.bWasUsingCloseHitSkip = bUsingCloseHitSkip;
	return InOutAimHelperState.TargetLocation;
}

bool UTimeThiefAimStatics::TraceLine(
	UWorld* World,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const TArray<AActor*>& ActorsToIgnore,
	FHitResult& OutHitResult,
	ECollisionChannel TraceChannel,
	bool bTraceComplex,
	bool bReturnPhysicalMaterial)
{
	if (!World)
	{
		return false;
	}

	const FCollisionQueryParams QueryParams = BuildTraceParams(ActorsToIgnore, bTraceComplex, bReturnPhysicalMaterial);
	return World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, TraceChannel, QueryParams);
}

bool UTimeThiefAimStatics::TraceLineWithParams(
	UWorld* World,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FCollisionQueryParams& QueryParams,
	FHitResult& OutHitResult,
	ECollisionChannel TraceChannel)
{
	if (!World)
	{
		return false;
	}

	return World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, TraceChannel, QueryParams);
}
