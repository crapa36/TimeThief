#include "Components/Wire/TimeThiefWireTargeting.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"

UTimeThiefWireTargeting::UTimeThiefWireTargeting()
{
	CollisionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	CollisionObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
}

void UTimeThiefWireTargeting::Initialize(ACharacter* InCharacter)
{
	CachedCharacter = InCharacter;
}

bool UTimeThiefWireTargeting::FindBestAnchorTarget(FVector& OutTargetLocation, const FVector& StartLocation, const FVector& AimDirection, float MaxLength)
{
	if (!IsValid(CachedCharacter)) return false;

	UWorld* World = CachedCharacter->GetWorld();
	if (!World) return false;

	const FVector EndLocation = StartLocation + AimDirection * MaxLength;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CachedCharacter);

	TArray<FHitResult> Candidates;

	FHitResult LineHit;
	if (World->LineTraceSingleByObjectType(LineHit, StartLocation, EndLocation, FCollisionObjectQueryParams(CollisionObjectTypes), QueryParams))
	{
		Candidates.Add(LineHit);
	}

	TArray<FHitResult> SweepHits;
	World->SweepMultiByObjectType(
		SweepHits, StartLocation, EndLocation, FQuat::Identity,
		FCollisionObjectQueryParams(CollisionObjectTypes),
		FCollisionShape::MakeSphere(AutoAimRadius),
		QueryParams
	);
	
	Candidates.Append(SweepHits);

	if (Candidates.IsEmpty()) return false;

	float BestLedgeDistanceSq = FLT_MAX;
	float BestSurfaceDistanceSq = FLT_MAX;
	FVector BestLedgeLocation = FVector::ZeroVector;
	FVector BestSurfaceLocation = FVector::ZeroVector;
	bool bFoundLedgeCandidate = false;
	bool bFoundSurfaceCandidate = false;

	for (const FHitResult& Hit : Candidates)
	{
		if (Hit.Component.IsValid() && Hit.Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap)
		{
			continue;
		}

		float DistanceToPlayer = FVector::Dist(StartLocation, Hit.ImpactPoint);
		if (DistanceToPlayer < MinTargetDistance) continue;

		if (!bAllowFloorAttachment && Hit.ImpactNormal.Z >= 0.7f) continue;

		bool bIsLedge = false;
		const FVector LedgeCheckStart = Hit.ImpactPoint + FVector(0, 0, LedgeCheckHeight);
		const FVector LedgeCheckEnd = LedgeCheckStart + (Hit.ImpactNormal * -20.0f);
		
		FHitResult LedgeHit;
		bool bLedgeBlocked = World->LineTraceSingleByObjectType(
			LedgeHit, LedgeCheckStart, LedgeCheckEnd,
			FCollisionObjectQueryParams(CollisionObjectTypes),
			QueryParams
		);

		if (!bLedgeBlocked)
		{
			bIsLedge = true;
		}

		const FVector CandidateLocation = bIsLedge ? Hit.ImpactPoint + FVector(0, 0, 5.0f) : FVector(Hit.ImpactPoint);
		const float DistanceToAimLine = FMath::PointDistToLine(CandidateLocation, AimDirection, StartLocation);
		const float DistanceToAimSq = FMath::Square(DistanceToAimLine);

		if (bIsLedge)
		{
			if (!bFoundLedgeCandidate || DistanceToAimSq < BestLedgeDistanceSq)
			{
				BestLedgeDistanceSq = DistanceToAimSq;
				BestLedgeLocation = CandidateLocation;
				bFoundLedgeCandidate = true;
			}
		}
		else if (!bFoundSurfaceCandidate || DistanceToAimSq < BestSurfaceDistanceSq)
		{
			BestSurfaceDistanceSq = DistanceToAimSq;
			BestSurfaceLocation = CandidateLocation;
			bFoundSurfaceCandidate = true;
		}
	}

	if (bFoundLedgeCandidate)
	{
		OutTargetLocation = BestLedgeLocation;
		return true;
	}

	if (bFoundSurfaceCandidate)
	{
		OutTargetLocation = BestSurfaceLocation;
		return true;
	}

	return false;
}

bool UTimeThiefWireTargeting::CheckAnchorCollision(const FVector& Start, const FVector& End, FHitResult& OutHit, AActor* IgnoredActor)
{
	if (!IsValid(CachedCharacter)) return false;
	UWorld* World = CachedCharacter->GetWorld();
	if (!World) return false;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(IgnoredActor);

	TArray<FHitResult> HitResults;
	bool bHit = World->SweepMultiByObjectType(
		HitResults, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(CollisionObjectTypes),
		FCollisionShape::MakeSphere(AnchorCollisionRadius),
		QueryParams
	);

	if (bHit)
	{
		bool bFoundBestHit = false;
		float BestHitTime = FLT_MAX;
		float BestHitDistanceSq = FLT_MAX;
		for (const FHitResult& Hit : HitResults)
		{
			if (Hit.Component.IsValid() && Hit.Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap)
			{
				continue;
			}

			const float HitDistanceSq = FVector::DistSquared(Start, Hit.ImpactPoint);
			if (!bFoundBestHit || Hit.Time < BestHitTime || (FMath::IsNearlyEqual(Hit.Time, BestHitTime) && HitDistanceSq < BestHitDistanceSq))
			{
				OutHit = Hit;
				BestHitTime = Hit.Time;
				BestHitDistanceSq = HitDistanceSq;
				bFoundBestHit = true;
			}
		}

		return bFoundBestHit;
	}

	return false;
}