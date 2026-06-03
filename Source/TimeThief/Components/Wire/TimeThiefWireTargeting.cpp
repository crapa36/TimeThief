#include "Components/Wire/TimeThiefWireTargeting.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "Utils/TimeThiefAimStatics.h"

namespace
{
	struct FWireScreenCandidate
	{
		FVector TargetLocation = FVector::ZeroVector;
		TWeakObjectPtr<UPrimitiveComponent> Component;
		TWeakObjectPtr<AActor> Actor;
		float DistanceToAimSq = FLT_MAX;
		float ScreenDistanceSq = FLT_MAX;
		bool bIsCenter = false;
	};

	bool IsUsableAnchorHit(const FHitResult& Hit)
	{
		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		return Hit.bBlockingHit
			&& !Hit.bStartPenetrating
			&& IsValid(HitComponent)
			&& HitComponent->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap;
	}

	bool IsUsableAnchorComponent(const UPrimitiveComponent* Component)
	{
		return IsValid(Component)
			&& Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap;
	}

	bool IsBetterScreenCandidate(const FWireScreenCandidate& Candidate, const FWireScreenCandidate& BestCandidate)
	{
		if (Candidate.ScreenDistanceSq < BestCandidate.ScreenDistanceSq)
		{
			return true;
		}
		if (FMath::IsNearlyEqual(Candidate.ScreenDistanceSq, BestCandidate.ScreenDistanceSq)
			&& Candidate.DistanceToAimSq < BestCandidate.DistanceToAimSq)
		{
			return true;
		}
		return FMath::IsNearlyEqual(Candidate.ScreenDistanceSq, BestCandidate.ScreenDistanceSq)
			&& FMath::IsNearlyEqual(Candidate.DistanceToAimSq, BestCandidate.DistanceToAimSq)
			&& Candidate.bIsCenter && !BestCandidate.bIsCenter;
	}
}

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

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CachedCharacter.Get());

	auto TraceFirstUsableHitByObjectType = [&](const FVector& TraceStart, const FVector& TraceEnd, FHitResult& OutHit) -> bool
	{
		TArray<FHitResult> Hits;
		if (!World->LineTraceMultiByObjectType(Hits, TraceStart, TraceEnd, ObjectQueryParams, QueryParams))
		{
			return false;
		}

		bool bFoundHit = false;
		float BestHitTime = FLT_MAX;
		for (const FHitResult& Hit : Hits)
		{
			if (!IsUsableAnchorHit(Hit))
			{
				continue;
			}

			if (!bFoundHit || Hit.Time < BestHitTime)
			{
				OutHit = Hit;
				BestHitTime = Hit.Time;
				bFoundHit = true;
			}
		}

		return bFoundHit;
	};

	auto AddCandidateFromHit = [&](const FHitResult& Hit, TArray<FWireScreenCandidate>& LedgeCandidates, TArray<FWireScreenCandidate>& SurfaceCandidates) -> bool
	{
		if (!IsUsableAnchorHit(Hit))
		{
			return false;
		}

		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		const FVector ImpactPoint = Hit.ImpactPoint;

		const FVector HorizontalNormal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.0f).GetSafeNormal();
		const FVector AbsHorizontalNormal = HorizontalNormal.GetAbs();
		const FVector Extent = HitComponent->Bounds.BoxExtent;
		const float ThicknessAlongNormal = (Extent.X * AbsHorizontalNormal.X) + (Extent.Y * AbsHorizontalNormal.Y);
		const float ProbeDistance = FMath::Clamp(ThicknessAlongNormal * ProbeDistanceScale, ProbeDistanceMin, ProbeDistanceMax);
		const FVector LedgeCheckStart = ImpactPoint + (HorizontalNormal * -ProbeDistance) + FVector(0.0f, 0.0f, LedgeCheckHeight * 0.5f);
		const FVector LedgeCheckEnd = LedgeCheckStart - FVector(0.0f, 0.0f, LedgeCheckHeight);

		FHitResult LedgeHit;
		const bool bIsLedge = TraceFirstUsableHitByObjectType(LedgeCheckStart, LedgeCheckEnd, LedgeHit)
			&& LedgeHit.ImpactNormal.Z >= LedgeMinNormalZ
			&& (LedgeHit.ImpactPoint.Z > ImpactPoint.Z + LedgeMinHeightDelta);

		const FVector TargetLocation = bIsLedge ? FVector(LedgeHit.ImpactPoint.X, LedgeHit.ImpactPoint.Y, LedgeHit.ImpactPoint.Z + 5.0f) : ImpactPoint;
		UPrimitiveComponent* TargetComponent = bIsLedge ? LedgeHit.GetComponent() : HitComponent;
		if (!IsValid(TargetComponent))
		{
			return false;
		}

		const float DistanceToStartSq = FVector::DistSquared(StartLocation, TargetLocation);
		if (DistanceToStartSq < MinDistanceSq || DistanceToStartSq > MaxDistanceSq)
		{
			return false;
		}

		FVector2D CandidateScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(TargetLocation, CandidateScreenPosition))
		{
			return false;
		}

		const float ScreenDistanceSq = FVector2D::DistSquared(CandidateScreenPosition, ScreenCenter);
		if (ScreenDistanceSq > MaxScreenDistanceSq)
		{
			return false;
		}

		const float DistanceToAimLine = FMath::PointDistToLine(TargetLocation, SafeAimDirection, StartLocation);

		FWireScreenCandidate Candidate;
		Candidate.TargetLocation = TargetLocation;
		Candidate.Component = TargetComponent;
		Candidate.Actor = bIsLedge ? LedgeHit.GetActor() : Hit.GetActor();
		Candidate.DistanceToAimSq = FMath::Square(DistanceToAimLine);
		Candidate.ScreenDistanceSq = ScreenDistanceSq;
		Candidate.bIsCenter = ScreenDistanceSq <= 1.0f;

		if (bIsLedge)
		{
			LedgeCandidates.Add(Candidate);
		}
		else
		{
			SurfaceCandidates.Add(Candidate);
		}

		return true;
	};

	auto TraceScreenSample = [&](const FVector2D& ScreenPosition, UPrimitiveComponent* ExpectedComponent, TArray<FWireScreenCandidate>& LedgeCandidates, TArray<FWireScreenCandidate>& SurfaceCandidates) -> bool
	{
		if (ScreenPosition.X < 0.0f || ScreenPosition.Y < 0.0f || ScreenPosition.X > ViewportX || ScreenPosition.Y > ViewportY)
		{
			return false;
		}

		FVector RayWorldOrigin;
		FVector RayWorldDirection;
		if (!PlayerController->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayWorldOrigin, RayWorldDirection))
		{
			return false;
		}

		const FVector SampleDir = UTimeThiefAimStatics::NormalizeAimDirection(RayWorldDirection);
		if (SampleDir.IsNearlyZero())
		{
			return false;
		}

		FHitResult Hit;
		if (!TraceFirstUsableHitByObjectType(RayWorldOrigin, RayWorldOrigin + SampleDir * MaxLength, Hit))
		{
			return false;
		}

		if (ExpectedComponent && Hit.GetComponent() != ExpectedComponent)
		{
			return false;
		}

		return AddCandidateFromHit(Hit, LedgeCandidates, SurfaceCandidates);
	};

	auto TryResolveBestCandidate = [&](const TArray<FWireScreenCandidate>& Candidates) -> bool
	{
		TArray<bool> RejectedCandidates;
		RejectedCandidates.Init(false, Candidates.Num());

		for (int32 AttemptIndex = 0; AttemptIndex < Candidates.Num(); ++AttemptIndex)
		{
			int32 BestCandidateIndex = INDEX_NONE;
			for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
			{
				if (RejectedCandidates[CandidateIndex])
				{
					continue;
				}

				if (BestCandidateIndex == INDEX_NONE || IsBetterScreenCandidate(Candidates[CandidateIndex], Candidates[BestCandidateIndex]))
				{
					BestCandidateIndex = CandidateIndex;
				}
			}

			if (BestCandidateIndex == INDEX_NONE)
			{
				return false;
			}

			const FWireScreenCandidate& BestCandidate = Candidates[BestCandidateIndex];
			FHitResult ScreenValidatedHit;
			if (TraceFirstUsableHitByObjectType(StartLocation, BestCandidate.TargetLocation, ScreenValidatedHit)
				&& ScreenValidatedHit.GetComponent() == BestCandidate.Component.Get())
			{
				OutTargetLocation = BestCandidate.TargetLocation;
				return true;
			}

			RejectedCandidates[BestCandidateIndex] = true;
		}

		return false;
	};

	auto TryResolveCandidateSet = [&](const TArray<FWireScreenCandidate>& LedgeCandidates, const TArray<FWireScreenCandidate>& SurfaceCandidates) -> bool
	{
		return !LedgeCandidates.IsEmpty() ? TryResolveBestCandidate(LedgeCandidates) : TryResolveBestCandidate(SurfaceCandidates);
	};

	auto TryRefineAroundScreenPoint = [&](const FVector2D& BaseScreenPosition, UPrimitiveComponent* ExpectedComponent, TArray<FWireScreenCandidate>& LedgeCandidates, TArray<FWireScreenCandidate>& SurfaceCandidates) -> bool
	{
		if (TraceScreenSample(BaseScreenPosition, ExpectedComponent, LedgeCandidates, SurfaceCandidates))
		{
			return true;
		}

		const float RefinePixelStep = FMath::Max(1.0f, ThinTargetRefinePixelStep);
		const float RefineRadius = FMath::Max(0.0f, ThinTargetRefineRadiusPx);
		const float RefineRadiusSq = FMath::Square(RefineRadius);
		const int32 NumRefineSteps = FMath::CeilToInt(RefineRadius / RefinePixelStep);

		for (int32 RadiusStep = 1; RadiusStep <= NumRefineSteps; ++RadiusStep)
		{
			for (int32 YStep = -RadiusStep; YStep <= RadiusStep; ++YStep)
			{
				for (int32 XStep = -RadiusStep; XStep <= RadiusStep; ++XStep)
				{
					if (FMath::Abs(XStep) != RadiusStep && FMath::Abs(YStep) != RadiusStep)
					{
						continue;
					}

					const FVector2D Offset(XStep * RefinePixelStep, YStep * RefinePixelStep);
					if (Offset.SizeSquared() > RefineRadiusSq)
					{
						continue;
					}

					const FVector2D SampleScreenPosition = BaseScreenPosition + Offset;
					if (FVector2D::DistSquared(SampleScreenPosition, ScreenCenter) > MaxScreenDistanceSq)
					{
						continue;
					}

					if (TraceScreenSample(SampleScreenPosition, ExpectedComponent, LedgeCandidates, SurfaceCandidates))
					{
						return true;
					}
				}
			}
		}

		return false;
	};

	auto CollectObjectGuidedCandidates = [&](TArray<FWireScreenCandidate>& LedgeCandidates, TArray<FWireScreenCandidate>& SurfaceCandidates)
	{
		if (ThinTargetObjectProbeRadius <= 0.0f)
		{
			return;
		}

		const float ProbeRadius = FMath::Max(ThinTargetObjectProbeRadius, 1.0f);
		const float ProbeHalfHeight = (MaxLength * 0.5f) + ProbeRadius;
		const FVector ProbeCenter = StartLocation + SafeAimDirection * (MaxLength * 0.5f);
		const FQuat ProbeRotation = FRotationMatrix::MakeFromZ(SafeAimDirection).ToQuat();

		TArray<FOverlapResult> ObjectOverlaps;
		const bool bHasObjectOverlaps = World->OverlapMultiByObjectType(
			ObjectOverlaps,
			ProbeCenter,
			ProbeRotation,
			ObjectQueryParams,
			FCollisionShape::MakeCapsule(ProbeRadius, ProbeHalfHeight),
			QueryParams);

		if (!bHasObjectOverlaps)
		{
			return;
		}

		TArray<UPrimitiveComponent*, TInlineAllocator<8>> ProcessedComponents;
		for (const FOverlapResult& Overlap : ObjectOverlaps)
		{
			UPrimitiveComponent* HitComponent = Overlap.GetComponent();
			if (!IsUsableAnchorComponent(HitComponent))
			{
				continue;
			}

			if (ProcessedComponents.Contains(HitComponent))
			{
				continue;
			}

			const FBoxSphereBounds& Bounds = HitComponent->Bounds;
			const FVector BoundsExtent = Bounds.BoxExtent;
			TArray<FVector, TInlineAllocator<9>> ProbePoints;
			ProbePoints.Add(Bounds.Origin);
			for (int32 XSign = -1; XSign <= 1; XSign += 2)
			{
				for (int32 YSign = -1; YSign <= 1; YSign += 2)
				{
					for (int32 ZSign = -1; ZSign <= 1; ZSign += 2)
					{
						ProbePoints.Add(Bounds.Origin + FVector(BoundsExtent.X * XSign, BoundsExtent.Y * YSign, BoundsExtent.Z * ZSign));
					}
				}
			}

			bool bHasScreenPoint = false;
			FVector2D BestScreenPosition = FVector2D::ZeroVector;
			float BestScreenDistanceSq = FLT_MAX;
			for (const FVector& ProbePoint : ProbePoints)
			{
				FVector2D ProbeScreenPosition;
				if (!PlayerController->ProjectWorldLocationToScreen(ProbePoint, ProbeScreenPosition))
				{
					continue;
				}

				const float ProbeScreenDistanceSq = FVector2D::DistSquared(ProbeScreenPosition, ScreenCenter);
				if (ProbeScreenDistanceSq < BestScreenDistanceSq)
				{
					BestScreenDistanceSq = ProbeScreenDistanceSq;
					BestScreenPosition = ProbeScreenPosition;
					bHasScreenPoint = true;
				}
			}

			if (!bHasScreenPoint || BestScreenDistanceSq > MaxScreenDistanceSq)
			{
				continue;
			}

			ProcessedComponents.Add(HitComponent);
			TryRefineAroundScreenPoint(BestScreenPosition, HitComponent, LedgeCandidates, SurfaceCandidates);
		}
	};

	auto CollectScreenGridCandidates = [&](TArray<FWireScreenCandidate>& LedgeCandidates, TArray<FWireScreenCandidate>& SurfaceCandidates)
	{
		const float PixelStep = FMath::Max(1.0f, ScreenSamplePixelStep);
		const int32 NumPitchSteps = FMath::Max(1, FMath::CeilToInt(ScaledScreenRadiusPx / PixelStep));
		const int32 NumYawSteps = FMath::Max(1, FMath::CeilToInt(ScaledScreenRadiusPx / PixelStep));

		for (int32 PitchStep = -NumPitchSteps; PitchStep <= NumPitchSteps; ++PitchStep)
		{
			for (int32 YawStep = -NumYawSteps; YawStep <= NumYawSteps; ++YawStep)
			{
				const float SampleOffsetX = YawStep * PixelStep;
				const float SampleOffsetY = PitchStep * PixelStep;
				const float SampleRadiusSq = FMath::Square(SampleOffsetX) + FMath::Square(SampleOffsetY);
				if (SampleRadiusSq > MaxScreenDistanceSq)
				{
					continue;
				}

				TraceScreenSample(ScreenCenter + FVector2D(SampleOffsetX, SampleOffsetY), nullptr, LedgeCandidates, SurfaceCandidates);
			}
		}
	};

	TArray<FWireScreenCandidate> ThinLedgeCandidates;
	TArray<FWireScreenCandidate> ThinSurfaceCandidates;
	ThinLedgeCandidates.Reserve(8);
	ThinSurfaceCandidates.Reserve(16);
	CollectObjectGuidedCandidates(ThinLedgeCandidates, ThinSurfaceCandidates);
	if (TryResolveCandidateSet(ThinLedgeCandidates, ThinSurfaceCandidates))
	{
		return true;
	}

	TArray<FWireScreenCandidate> ScreenLedgeCandidates;
	TArray<FWireScreenCandidate> ScreenSurfaceCandidates;
	ScreenLedgeCandidates.Reserve(32);
	ScreenSurfaceCandidates.Reserve(64);
	CollectScreenGridCandidates(ScreenLedgeCandidates, ScreenSurfaceCandidates);
	return TryResolveCandidateSet(ScreenLedgeCandidates, ScreenSurfaceCandidates);
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
			UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!Hit.bBlockingHit || Hit.bStartPenetrating || !IsValid(HitComponent))
			{
				continue;
			}

			if (HitComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap)
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
