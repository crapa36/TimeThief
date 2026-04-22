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
		OutViewDirection = CameraRotation.Vector().GetSafeNormal();
		return true;
	}

	OutViewLocation = Pawn->GetPawnViewLocation();
	OutViewDirection = Pawn->GetBaseAimRotation().Vector().GetSafeNormal();
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

	return TraceFromView(Pawn->GetWorld(), ViewLocation, ViewDirection, Range, ActorsToIgnore, OutHitResult, OutTraceEnd, TraceChannel, bTraceComplex, bReturnPhysicalMaterial);
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
	const FVector SafeDirection = ViewDirection.GetSafeNormal();
	OutTraceEnd = ViewLocation + (SafeDirection * Range);
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

