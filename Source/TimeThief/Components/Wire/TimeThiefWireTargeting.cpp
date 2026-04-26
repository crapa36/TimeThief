#include "Components/Wire/TimeThiefWireTargeting.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Utils/TimeThiefAimStatics.h"

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
	const FVector SafeAimDirection = UTimeThiefAimStatics::NormalizeAimDirection(AimDirection);
	if (SafeAimDirection.IsNearlyZero()) return false;

	UWorld* World = CachedCharacter->GetWorld();
	if (!World) return false;
	APlayerController* PlayerController = Cast<APlayerController>(CachedCharacter->GetController());
	if (!PlayerController) return false;
	const FVector CharacterLocation = CachedCharacter->GetActorLocation();

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PlayerController->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0) return false;

	const FVector2D ScreenCenter(ViewportX * 0.5f, ViewportY * 0.5f);
	const float ResolutionScale = FMath::Min(static_cast<float>(ViewportX), static_cast<float>(ViewportY)) / 1080.0f;
	const float ScaledScreenRadiusPx = ScreenTargetingRadiusPx * FMath::Max(ResolutionScale, 0.1f);
	const float MaxScreenDistanceSq = FMath::Square(ScaledScreenRadiusPx);
	const float MinDistanceSq = FMath::Square(MinTargetDistance);
	const float MaxDistanceSq = FMath::Square(MaxLength);

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(CachedCharacter.Get());
	const FCollisionObjectQueryParams ObjectQueryParams(CollisionObjectTypes);

	struct FScreenCandidate
	{
		FVector TargetLocation = FVector::ZeroVector;
		TWeakObjectPtr<UPrimitiveComponent> Component;
		TWeakObjectPtr<AActor> Actor;
		float DistanceToAimSq = FLT_MAX;
		float ScreenDistanceSq = FLT_MAX;
		bool bIsCenter = false;
	};

	TArray<FScreenCandidate> LedgeCandidates;
	TArray<FScreenCandidate> SurfaceCandidates;
	LedgeCandidates.Reserve(8);
	SurfaceCandidates.Reserve(16);
	
	const float PixelStep = FMath::Max(1.0f, ScreenSamplePixelStep);
	const int32 NumPitchSteps = FMath::Max(1, FMath::CeilToInt(ScaledScreenRadiusPx / PixelStep));
	const int32 NumYawSteps = FMath::Max(1, FMath::CeilToInt(ScaledScreenRadiusPx / PixelStep));

	for (int32 PitchStep = -NumPitchSteps; PitchStep <= NumPitchSteps; ++PitchStep)
	{
		for (int32 YawStep = -NumYawSteps; YawStep <= NumYawSteps; ++YawStep)
		{
			const bool bIsCenter = PitchStep == 0 && YawStep == 0;
			const float SampleOffsetX = YawStep * PixelStep;
			const float SampleOffsetY = PitchStep * PixelStep;
			const float SampleRadiusSq = FMath::Square(SampleOffsetX) + FMath::Square(SampleOffsetY);
			if (SampleRadiusSq > MaxScreenDistanceSq)
			{
				continue;
			}

			const float SampleScreenX = ScreenCenter.X + SampleOffsetX;
			const float SampleScreenY = ScreenCenter.Y + SampleOffsetY;

			FVector RayWorldOrigin;
			FVector RayWorldDirection;
			if (!PlayerController->DeprojectScreenPositionToWorld(SampleScreenX, SampleScreenY, RayWorldOrigin, RayWorldDirection))
			{
				continue;
			}

			const FVector SampleDir = UTimeThiefAimStatics::NormalizeAimDirection(RayWorldDirection);
			if (SampleDir.IsNearlyZero())
			{
				continue;
			}

			const FVector TraceStart = RayWorldOrigin;
			const FVector TraceEnd = TraceStart + SampleDir * MaxLength;

			FHitResult Hit;
			if (!UTimeThiefAimStatics::TraceLineByObjectType(
				World,
				TraceStart,
				TraceEnd,
				ObjectQueryParams,
				ActorsToIgnore,
				Hit
			))
			{
				continue;
			}

			UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!IsValid(HitComponent))
			{
				continue;
			}

			if (HitComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap)
			{
				continue;
			}

			const FVector ImpactPoint = Hit.ImpactPoint;
			const float DistanceToPlayerSq = FVector::DistSquared(CharacterLocation, ImpactPoint);
			if (DistanceToPlayerSq < MinDistanceSq || DistanceToPlayerSq > MaxDistanceSq)
			{
				continue;
			}

			FVector2D CandidateScreenPosition;
			if (!PlayerController->ProjectWorldLocationToScreen(ImpactPoint, CandidateScreenPosition))
			{
				continue;
			}

			const float ScreenDistanceSq = FVector2D::DistSquared(CandidateScreenPosition, ScreenCenter);
			if (ScreenDistanceSq > MaxScreenDistanceSq)
			{
				continue;
			}

			const float DistanceToAimLine = FMath::PointDistToLine(ImpactPoint, SafeAimDirection, StartLocation);
			const float DistanceToAimSq = FMath::Square(DistanceToAimLine);

			const FVector HorizontalNormal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.0f).GetSafeNormal();
			const FVector AbsHorizontalNormal = HorizontalNormal.GetAbs();
			const FVector Extent = HitComponent->Bounds.BoxExtent;
			const float ThicknessAlongNormal = (Extent.X * AbsHorizontalNormal.X) + (Extent.Y * AbsHorizontalNormal.Y);
			const float ProbeDistance = FMath::Clamp(ThicknessAlongNormal * ProbeDistanceScale, ProbeDistanceMin, ProbeDistanceMax);
			const FVector LedgeCheckStart = ImpactPoint + (HorizontalNormal * -ProbeDistance) + FVector(0.0f, 0.0f, LedgeCheckHeight * 0.5f);
			const FVector LedgeCheckEnd = LedgeCheckStart - FVector(0.0f, 0.0f, LedgeCheckHeight);

			FHitResult LedgeHit;
			const bool bIsLedge = UTimeThiefAimStatics::TraceLineByObjectType(
				World,
				LedgeCheckStart,
				LedgeCheckEnd,
				ObjectQueryParams,
				ActorsToIgnore,
				LedgeHit
			) && LedgeHit.ImpactNormal.Z >= LedgeMinNormalZ && (LedgeHit.ImpactPoint.Z > ImpactPoint.Z + LedgeMinHeightDelta);

			FScreenCandidate Candidate;
			Candidate.TargetLocation = bIsLedge ? FVector(LedgeHit.ImpactPoint.X, LedgeHit.ImpactPoint.Y, ImpactPoint.Z + 5.0f) : ImpactPoint;
			Candidate.Component = HitComponent;
			Candidate.Actor = Hit.GetActor();
			Candidate.DistanceToAimSq = DistanceToAimSq;
			Candidate.ScreenDistanceSq = ScreenDistanceSq;
			Candidate.bIsCenter = bIsCenter;

			if (bIsLedge)
			{
				LedgeCandidates.Add(Candidate);
			}
			else
			{
				SurfaceCandidates.Add(Candidate);
			}
		}
	}

	const TArray<FScreenCandidate>& ActiveCandidates = !LedgeCandidates.IsEmpty() ? LedgeCandidates : SurfaceCandidates;

	if (ActiveCandidates.IsEmpty())
	{
		return false;
	}

	int32 BestCandidateIndex = INDEX_NONE;
	for (int32 CandidateIndex = 0; CandidateIndex < ActiveCandidates.Num(); ++CandidateIndex)
	{
		const FScreenCandidate& Candidate = ActiveCandidates[CandidateIndex];
		if (BestCandidateIndex == INDEX_NONE)
		{
			BestCandidateIndex = CandidateIndex;
			continue;
		}

		const FScreenCandidate& BestCandidate = ActiveCandidates[BestCandidateIndex];
		if (Candidate.DistanceToAimSq < BestCandidate.DistanceToAimSq)
		{
			BestCandidateIndex = CandidateIndex;
			continue;
		}
		if (FMath::IsNearlyEqual(Candidate.DistanceToAimSq, BestCandidate.DistanceToAimSq) && Candidate.ScreenDistanceSq < BestCandidate.ScreenDistanceSq)
		{
			BestCandidateIndex = CandidateIndex;
			continue;
		}
		if (FMath::IsNearlyEqual(Candidate.DistanceToAimSq, BestCandidate.DistanceToAimSq)
			&& FMath::IsNearlyEqual(Candidate.ScreenDistanceSq, BestCandidate.ScreenDistanceSq)
			&& Candidate.bIsCenter && !BestCandidate.bIsCenter)
		{
			BestCandidateIndex = CandidateIndex;
		}
	}

	if (BestCandidateIndex == INDEX_NONE)
	{
		return false;
	}

	const FScreenCandidate& BestCandidate = ActiveCandidates[BestCandidateIndex];
	FHitResult ScreenValidatedHit;
	const bool bHasBlockingHit = UTimeThiefAimStatics::TraceLineByObjectType(
		World,
		StartLocation,
		BestCandidate.TargetLocation,
		ObjectQueryParams,
		ActorsToIgnore,
		ScreenValidatedHit
	);

	if (!bHasBlockingHit)
	{
		return false;
	}

	if (ScreenValidatedHit.Component == BestCandidate.Component || ScreenValidatedHit.GetActor() == BestCandidate.Actor)
	{
		OutTargetLocation = BestCandidate.TargetLocation;
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
