#include "Utils/TimeThiefAimStatics.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	FCollisionQueryParams BuildTraceParams(const TArray<AActor*>& ActorsToIgnore, bool bTraceComplex, bool bReturnPhysicalMaterial)
	{
		FCollisionQueryParams QueryParams;
		QueryParams.bTraceComplex = bTraceComplex;
		QueryParams.bReturnPhysicalMaterial = bReturnPhysicalMaterial;

		for (AActor* ActorToIgnore : ActorsToIgnore)
		{
			if (ActorToIgnore)
			{
				QueryParams.AddIgnoredActor(ActorToIgnore);
			}
		}

		return QueryParams;
	}
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

FRotator UTimeThiefAimStatics::ResolveRelativeAimRotation(
	const FRotator& BaseRotation,
	const FVector& AimDirection,
	const FRotator& FallbackRotation)
{
	const FRotator AimRotation = ResolveAimRotationFromDirection(AimDirection, FallbackRotation);
	return (AimRotation - BaseRotation).GetNormalized();
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

	TArray<AActor*> ActualActorsToIgnore = ActorsToIgnore;
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
	OutTraceEnd = ResolveAimTargetLocation(ViewLocation, ViewDirection, Range);
	return TraceLine(World, ViewLocation, OutTraceEnd, ActorsToIgnore, OutHitResult, TraceChannel, bTraceComplex, bReturnPhysicalMaterial);
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

bool UTimeThiefAimStatics::TraceLineByObjectType(
	UWorld* World,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FCollisionObjectQueryParams& ObjectQueryParams,
	const TArray<AActor*>& ActorsToIgnore,
	FHitResult& OutHitResult,
	bool bTraceComplex,
	bool bReturnPhysicalMaterial)
{
	if (!World)
	{
		return false;
	}

	const FCollisionQueryParams QueryParams = BuildTraceParams(ActorsToIgnore, bTraceComplex, bReturnPhysicalMaterial);
	return World->LineTraceSingleByObjectType(OutHitResult, TraceStart, TraceEnd, ObjectQueryParams, QueryParams);
}