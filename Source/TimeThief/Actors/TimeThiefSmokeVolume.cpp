#include "Actors/TimeThiefSmokeVolume.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "Smoke/TimeThiefSmokeWorldSubsystem.h"
#include "TimeThief.h"
#include "TimeThiefSmokeParameterDefaults.h"
#include "TimeThiefSmokeTestBridge.h"
#include "Weapon/TimeThiefRocketProjectile.h"
#include "Weapon/TimeThiefThrowableProjectile.h"
#include "WorldCollision.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#endif

namespace TimeThiefSmokeVolume
{
	const FName ServerCollisionTag(TEXT("ServerCollision"));
	const FName BlockProjectileTag(TEXT("BlockProjectile"));
	const FName BlockMovementTag(TEXT("BlockMovement"));
	const FName IgnoreTag(TEXT("Ignore"));

	struct FSmokeObstacleMotion
	{
		FVector LinearVelocity = FVector::ZeroVector;
		FVector AngularVelocity = FVector::ZeroVector;
	};

	int32 FlattenCellCoord(const FIntVector& Coord, const FIntVector& CellGrid)
	{
		return Coord.X + Coord.Y * CellGrid.X + Coord.Z * CellGrid.X * CellGrid.Y;
	}

	float SmoothStep01(float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	FVector MakeObstacleMaskQueryExtent(const FVector& CellHalfExtent, float Inflation)
	{
		return FVector(Inflation).ComponentMax(CellHalfExtent * TimeThiefSmokeParameterDefaults::ObstacleMaskCellFootprintRatio);
	}

	FBox MakeWorldAabbFromOrientedBox(const FVector& Center, const FQuat& Rotation, const FVector& Extent)
	{
		FBox Bounds(EForceInit::ForceInit);
		for (int32 Z = -1; Z <= 1; Z += 2)
		{
			for (int32 Y = -1; Y <= 1; Y += 2)
			{
				for (int32 X = -1; X <= 1; X += 2)
				{
					Bounds += Center + Rotation.RotateVector(FVector(Extent.X * X, Extent.Y * Y, Extent.Z * Z));
				}
			}
		}

		return Bounds;
	}

	bool IsIgnoredSmokeObstacleActor(const AActor* Actor, const ATimeThiefSmokeVolume* SmokeVolume)
	{
		if (!Actor || Actor == SmokeVolume || Actor->IsA<ATimeThiefSmokeVolume>())
		{
			return true;
		}

		if (SmokeVolume && (Actor == SmokeVolume->GetOwner() || Actor == SmokeVolume->GetInstigator()))
		{
			return true;
		}

		return Actor->IsA<APawn>() ||
			Actor->IsA<ATimeThiefThrowableProjectile>() ||
			Actor->IsA<ATimeThiefRocketProjectile>();
	}

	bool IsServerCollisionShape(const UPrimitiveComponent* PrimitiveComponent)
	{
		const AActor* OwnerActor = PrimitiveComponent ? PrimitiveComponent->GetOwner() : nullptr;
		return OwnerActor && OwnerActor->ActorHasTag(ServerCollisionTag) && PrimitiveComponent->IsA<UShapeComponent>();
	}

	bool IsHardSmokeObstacleComponent(const UPrimitiveComponent* PrimitiveComponent, const ATimeThiefSmokeVolume* SmokeVolume)
	{
		if (!PrimitiveComponent || PrimitiveComponent->ComponentHasTag(IgnoreTag) || !PrimitiveComponent->IsQueryCollisionEnabled())
		{
			return false;
		}

		const AActor* OwnerActor = PrimitiveComponent->GetOwner();
		if (IsIgnoredSmokeObstacleActor(OwnerActor, SmokeVolume))
		{
			return false;
		}

		const bool bHasProjectileTag = PrimitiveComponent->ComponentHasTag(BlockProjectileTag);
		const bool bIsServerCollisionShape = IsServerCollisionShape(PrimitiveComponent);
		if (bIsServerCollisionShape)
		{
			return bHasProjectileTag;
		}

		if (bHasProjectileTag)
		{
			return true;
		}

		if (PrimitiveComponent->GetCollisionObjectType() == ECC_WorldStatic &&
			PrimitiveComponent->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block)
		{
			return true;
		}

		return PrimitiveComponent->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Block;
	}

	bool IsStaticSmokeObstacleComponent(const UPrimitiveComponent* PrimitiveComponent)
	{
		return PrimitiveComponent &&
			(PrimitiveComponent->Mobility == EComponentMobility::Static ||
				PrimitiveComponent->GetCollisionObjectType() == ECC_WorldStatic);
	}

	bool HasSupportedSmokeObstacleGeometry(const UPrimitiveComponent* PrimitiveComponent)
	{
		if (PrimitiveComponent &&
			(PrimitiveComponent->IsA<USphereComponent>() ||
				PrimitiveComponent->IsA<UCapsuleComponent>() ||
				PrimitiveComponent->IsA<UBoxComponent>()))
		{
			return true;
		}

		UBodySetup* BodySetup = PrimitiveComponent ? const_cast<UPrimitiveComponent*>(PrimitiveComponent)->GetBodySetup() : nullptr;
		return BodySetup &&
			(BodySetup->AggGeom.SphereElems.Num() > 0 ||
				BodySetup->AggGeom.BoxElems.Num() > 0 ||
				BodySetup->AggGeom.SphylElems.Num() > 0);
	}

	void AddSmokeObstacleCandidates(
		const TArray<FOverlapResult>& Overlaps,
		TSet<UPrimitiveComponent*>& AddedCandidates,
		TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutCandidates)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
			if (PrimitiveComponent && !AddedCandidates.Contains(PrimitiveComponent))
			{
				AddedCandidates.Add(PrimitiveComponent);
				OutCandidates.Add(PrimitiveComponent);
			}
		}
	}

	struct FSmokeObstacleCandidateCacheEntry
	{
		uint64 Key = 0;
		TArray<TWeakObjectPtr<UPrimitiveComponent>> Candidates;
		uint32 LastUsedFrame = 0;
	};

	TArray<FSmokeObstacleCandidateCacheEntry> GObstacleCandidateCache;

	void MixObstacleQueryHash(uint64& Hash, uint64 Value)
	{
		Hash ^= Value + 0x9e3779b97f4a7c15ull + (Hash << 6) + (Hash >> 2);
	}

	void MixObstacleQueryFloat(uint64& Hash, double Value, double UnitsPerCm = 10.0)
	{
		MixObstacleQueryHash(Hash, static_cast<uint64>(static_cast<int64>(FMath::RoundToDouble(Value * UnitsPerCm))));
	}

	void MixObstacleQueryVector(uint64& Hash, const FVector& Value, double UnitsPerCm = 10.0)
	{
		MixObstacleQueryFloat(Hash, Value.X, UnitsPerCm);
		MixObstacleQueryFloat(Hash, Value.Y, UnitsPerCm);
		MixObstacleQueryFloat(Hash, Value.Z, UnitsPerCm);
	}

	void MixObstacleQueryTransform(uint64& Hash, const FTransform& Transform)
	{
		MixObstacleQueryVector(Hash, Transform.GetLocation());
		const FQuat Rotation = Transform.GetRotation();
		MixObstacleQueryFloat(Hash, Rotation.X, 100000.0);
		MixObstacleQueryFloat(Hash, Rotation.Y, 100000.0);
		MixObstacleQueryFloat(Hash, Rotation.Z, 100000.0);
		MixObstacleQueryFloat(Hash, Rotation.W, 100000.0);
		MixObstacleQueryVector(Hash, Transform.GetScale3D(), 10000.0);
	}

	uint64 MakeObstacleCandidateQueryCacheKey(
		const UWorld* World,
		const ATimeThiefSmokeVolume* SmokeVolume,
		const FTransform& SmokeTransform,
		const FVector& BoundsExtent,
		const FVector& QueryExtent)
	{
		uint64 Hash = 0xcbf29ce484222325ull;
		MixObstacleQueryHash(Hash, reinterpret_cast<uint64>(World));
		MixObstacleQueryTransform(Hash, SmokeTransform);
		MixObstacleQueryVector(Hash, BoundsExtent);
		MixObstacleQueryVector(Hash, QueryExtent);
		if (const AActor* OwnerActor = SmokeVolume ? SmokeVolume->GetOwner() : nullptr)
		{
			MixObstacleQueryHash(Hash, static_cast<uint64>(OwnerActor->GetUniqueID()));
		}
		if (const APawn* InstigatorPawn = SmokeVolume ? SmokeVolume->GetInstigator() : nullptr)
		{
			MixObstacleQueryHash(Hash, static_cast<uint64>(InstigatorPawn->GetUniqueID()));
		}
		return Hash;
	}

	const FSmokeObstacleCandidateCacheEntry* FindObstacleCandidateCacheEntry(uint64 Key)
	{
		const uint32 CurrentFrame = static_cast<uint32>(GFrameCounter);
		for (FSmokeObstacleCandidateCacheEntry& Entry : GObstacleCandidateCache)
		{
			if (Entry.Key == Key && Entry.LastUsedFrame == CurrentFrame)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	void StoreObstacleCandidateCacheEntry(uint64 Key, const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Candidates)
	{
		FSmokeObstacleCandidateCacheEntry* Entry = nullptr;
		if (GObstacleCandidateCache.Num() >= TimeThiefSmokeParameterDefaults::ObstacleMaskCacheMaxEntries)
		{
			int32 OldestIndex = 0;
			uint32 OldestFrame = GObstacleCandidateCache[0].LastUsedFrame;
			for (int32 EntryIndex = 1; EntryIndex < GObstacleCandidateCache.Num(); ++EntryIndex)
			{
				if (GObstacleCandidateCache[EntryIndex].LastUsedFrame < OldestFrame)
				{
					OldestFrame = GObstacleCandidateCache[EntryIndex].LastUsedFrame;
					OldestIndex = EntryIndex;
				}
			}
			Entry = &GObstacleCandidateCache[OldestIndex];
		}
		else
		{
			Entry = &GObstacleCandidateCache.AddDefaulted_GetRef();
		}

		Entry->Key = Key;
		Entry->Candidates = Candidates;
		Entry->LastUsedFrame = static_cast<uint32>(GFrameCounter);
	}

	void QuerySmokeObstacleCandidates(
		UWorld* World,
		const ATimeThiefSmokeVolume* SmokeVolume,
		const FTransform& SmokeTransform,
		const FVector& BoundsExtent,
		const FVector& QueryExtent,
		const FCollisionQueryParams& QueryParams,
		TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutCandidates)
	{
		if (!World)
		{
			return;
		}

		TArray<FOverlapResult> Overlaps;
		TSet<UPrimitiveComponent*> AddedCandidates;
		AddedCandidates.Reserve(OutCandidates.Num());
		for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : OutCandidates)
		{
			if (UPrimitiveComponent* PrimitiveComponent = Candidate.Get())
			{
				AddedCandidates.Add(PrimitiveComponent);
			}
		}

		const uint64 QueryCacheKey = MakeObstacleCandidateQueryCacheKey(World, SmokeVolume, SmokeTransform, BoundsExtent, QueryExtent);
		if (const FSmokeObstacleCandidateCacheEntry* CacheEntry = FindObstacleCandidateCacheEntry(QueryCacheKey))
		{
			for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : CacheEntry->Candidates)
			{
				UPrimitiveComponent* PrimitiveComponent = Candidate.Get();
				if (PrimitiveComponent && !AddedCandidates.Contains(PrimitiveComponent))
				{
					AddedCandidates.Add(PrimitiveComponent);
					OutCandidates.Add(PrimitiveComponent);
				}
			}
			return;
		}

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

		const FCollisionShape BoundsShape = FCollisionShape::MakeBox(BoundsExtent + QueryExtent);
		World->OverlapMultiByObjectType(
			Overlaps,
			SmokeTransform.GetLocation(),
			SmokeTransform.GetRotation(),
			ObjectQueryParams,
			BoundsShape,
			QueryParams);
		OutCandidates.Reserve(OutCandidates.Num() + Overlaps.Num());
		AddSmokeObstacleCandidates(Overlaps, AddedCandidates, OutCandidates);
		StoreObstacleCandidateCacheEntry(QueryCacheKey, OutCandidates);
	}

	struct FSmokeObstacleFieldCacheEntry
	{
		uint64 Key = 0;
		TArray<FTimeThiefSmokeObstaclePrimitive> ObstaclePrimitives;
		int32 ObstacleFieldResolution = 0;
		uint32 LastUsedFrame = 0;
		bool bHasSolidObstacleField = false;
	};

	TArray<FSmokeObstacleFieldCacheEntry> GObstacleFieldCache;

	void MixObstacleMaskHash(uint64& Hash, uint64 Value)
	{
		Hash ^= Value + 0x9e3779b97f4a7c15ull + (Hash << 6) + (Hash >> 2);
	}

	void MixObstacleMaskFloat(uint64& Hash, double Value, double UnitsPerCm = 10.0)
	{
		MixObstacleMaskHash(Hash, static_cast<uint64>(static_cast<int64>(FMath::RoundToDouble(Value * UnitsPerCm))));
	}

	void MixObstacleMaskVector(uint64& Hash, const FVector& Value, double UnitsPerCm = 10.0)
	{
		MixObstacleMaskFloat(Hash, Value.X, UnitsPerCm);
		MixObstacleMaskFloat(Hash, Value.Y, UnitsPerCm);
		MixObstacleMaskFloat(Hash, Value.Z, UnitsPerCm);
	}

	void MixObstacleMaskTransform(uint64& Hash, const FTransform& Transform)
	{
		MixObstacleMaskVector(Hash, Transform.GetLocation());
		const FQuat Rotation = Transform.GetRotation();
		MixObstacleMaskFloat(Hash, Rotation.X, 100000.0);
		MixObstacleMaskFloat(Hash, Rotation.Y, 100000.0);
		MixObstacleMaskFloat(Hash, Rotation.Z, 100000.0);
		MixObstacleMaskFloat(Hash, Rotation.W, 100000.0);
		MixObstacleMaskVector(Hash, Transform.GetScale3D(), 10000.0);
	}

	uint64 MakeObstacleMaskCacheKey(
		const UWorld* World,
		const FTransform& SmokeTransform,
		const FVector& NaturalBoundsExtent,
		const FVector& RenderBoundsExtent,
		int32 Resolution,
		const TArray<TWeakObjectPtr<UPrimitiveComponent>>& StaticObstacleCandidates)
	{
		uint64 Hash = 0xcbf29ce484222325ull;
		MixObstacleMaskHash(Hash, reinterpret_cast<uint64>(World));
		MixObstacleMaskHash(Hash, static_cast<uint64>(Resolution));
		MixObstacleMaskHash(Hash, static_cast<uint64>(TimeThiefSmokeParameterDefaults::MaxObstaclePrimitives));
		MixObstacleMaskHash(Hash, TimeThiefSmokeParameterDefaults::bUseStaticObstacleMask ? 1ull : 0ull);
		MixObstacleMaskTransform(Hash, SmokeTransform);
		MixObstacleMaskVector(Hash, NaturalBoundsExtent);
		MixObstacleMaskVector(Hash, RenderBoundsExtent);

		for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : StaticObstacleCandidates)
		{
			const UPrimitiveComponent* PrimitiveComponent = Candidate.Get();
			if (!PrimitiveComponent)
			{
				continue;
			}

			MixObstacleMaskHash(Hash, static_cast<uint64>(PrimitiveComponent->GetUniqueID()));
			MixObstacleMaskHash(Hash, static_cast<uint64>(PrimitiveComponent->GetCollisionEnabled()));
			MixObstacleMaskHash(Hash, static_cast<uint64>(PrimitiveComponent->GetCollisionResponseToChannel(ECC_WorldDynamic)));
			MixObstacleMaskHash(Hash, static_cast<uint64>(PrimitiveComponent->GetCollisionResponseToChannel(ECC_Visibility)));
			MixObstacleMaskTransform(Hash, PrimitiveComponent->GetComponentTransform());
			const FBox ComponentBounds = PrimitiveComponent->Bounds.GetBox();
			MixObstacleMaskVector(Hash, ComponentBounds.Min);
			MixObstacleMaskVector(Hash, ComponentBounds.Max);
		}

		return Hash;
	}

	const FSmokeObstacleFieldCacheEntry* FindObstacleFieldCacheEntry(uint64 Key)
	{
		for (FSmokeObstacleFieldCacheEntry& Entry : GObstacleFieldCache)
		{
			if (Entry.Key == Key)
			{
				Entry.LastUsedFrame = static_cast<uint32>(GFrameCounter);
				return &Entry;
			}
		}

		return nullptr;
	}

	void StoreObstacleFieldCacheEntry(
		uint64 Key,
		const TArray<FTimeThiefSmokeObstaclePrimitive>& ObstaclePrimitives,
		int32 ObstacleFieldResolution,
		bool bHasSolidObstacleField)
	{
		FSmokeObstacleFieldCacheEntry* Entry = nullptr;
		if (GObstacleFieldCache.Num() >= TimeThiefSmokeParameterDefaults::ObstacleMaskCacheMaxEntries)
		{
			int32 OldestIndex = 0;
			uint32 OldestFrame = GObstacleFieldCache[0].LastUsedFrame;
			for (int32 EntryIndex = 1; EntryIndex < GObstacleFieldCache.Num(); ++EntryIndex)
			{
				if (GObstacleFieldCache[EntryIndex].LastUsedFrame < OldestFrame)
				{
					OldestFrame = GObstacleFieldCache[EntryIndex].LastUsedFrame;
					OldestIndex = EntryIndex;
				}
			}
			Entry = &GObstacleFieldCache[OldestIndex];
		}
		else
		{
			Entry = &GObstacleFieldCache.AddDefaulted_GetRef();
		}

		Entry->Key = Key;
		Entry->ObstaclePrimitives = ObstaclePrimitives;
		Entry->ObstacleFieldResolution = ObstacleFieldResolution;
		Entry->LastUsedFrame = static_cast<uint32>(GFrameCounter);
		Entry->bHasSolidObstacleField = bHasSolidObstacleField;
	}

	float BoxSdf(const FVector& LocalPosition, const FVector& Extents)
	{
		const FVector Delta = LocalPosition.GetAbs() - Extents.ComponentMax(FVector(1.0));
		const FVector OutsideDelta(
			FMath::Max(Delta.X, 0.0),
			FMath::Max(Delta.Y, 0.0),
			FMath::Max(Delta.Z, 0.0));
		return OutsideDelta.Size() + FMath::Min(FMath::Max3(Delta.X, Delta.Y, Delta.Z), 0.0);
	}

	float EvaluateObstaclePrimitiveSdf(const FTimeThiefSmokeObstaclePrimitive& Primitive, const FVector& LocalPosition)
	{
		const FVector Center(Primitive.CenterRadius.X, Primitive.CenterRadius.Y, Primitive.CenterRadius.Z);
		const float Radius = Primitive.CenterRadius.W;
		const FVector Extents(Primitive.ExtentsShape.X, Primitive.ExtentsShape.Y, Primitive.ExtentsShape.Z);
		const ETimeThiefSmokeObstaclePrimitiveShape Shape = static_cast<ETimeThiefSmokeObstaclePrimitiveShape>(FMath::RoundToInt(Primitive.ExtentsShape.W));
		const FVector Delta = LocalPosition - Center;
		switch (Shape)
		{
		case ETimeThiefSmokeObstaclePrimitiveShape::Sphere:
			return Delta.Size() - Radius;
		case ETimeThiefSmokeObstaclePrimitiveShape::Capsule:
		{
			const FVector Axis = FVector(Primitive.AxisHalfLength.X, Primitive.AxisHalfLength.Y, Primitive.AxisHalfLength.Z).GetSafeNormal();
			const float HalfSegment = FMath::Max(Primitive.AxisHalfLength.W, 0.0f);
			const float Along = FMath::Clamp(FVector::DotProduct(Delta, Axis), -HalfSegment, HalfSegment);
			return (Delta - Axis * Along).Size() - Radius;
		}
		case ETimeThiefSmokeObstaclePrimitiveShape::Box:
		{
			const FQuat Rotation(Primitive.Rotation.X, Primitive.Rotation.Y, Primitive.Rotation.Z, Primitive.Rotation.W);
			return BoxSdf(Rotation.Inverse().RotateVector(Delta), Extents);
		}
		case ETimeThiefSmokeObstaclePrimitiveShape::Aabb:
		default:
			return BoxSdf(Delta, Extents);
		}
	}

	void SetObstaclePrimitiveMotion(FTimeThiefSmokeObstaclePrimitive& Primitive, const FSmokeObstacleMotion& Motion)
	{
		Primitive.Velocity = FVector4f(Motion.LinearVelocity.X, Motion.LinearVelocity.Y, Motion.LinearVelocity.Z, 0.0f);
		Primitive.AngularVelocity = FVector4f(Motion.AngularVelocity.X, Motion.AngularVelocity.Y, Motion.AngularVelocity.Z, 0.0f);
	}

	bool AreObstacleTransformsNearlyEqual(const FTransform& Left, const FTransform& Right)
	{
		return Left.GetLocation().Equals(Right.GetLocation(), 0.1) &&
			Left.GetScale3D().Equals(Right.GetScale3D(), 0.001) &&
			Left.GetRotation().AngularDistance(Right.GetRotation()) <= 0.001;
	}

	FSmokeObstacleMotion MakeSmokeLocalObstacleMotion(
		const FTransform& PreviousWorldTransform,
		const FTransform& CurrentWorldTransform,
		const FTransform& SmokeTransform,
		float DeltaTime)
	{
		FSmokeObstacleMotion Motion;
		if (DeltaTime <= KINDA_SMALL_NUMBER)
		{
			return Motion;
		}

		const FVector WorldLinearVelocity = (CurrentWorldTransform.GetLocation() - PreviousWorldTransform.GetLocation()) / DeltaTime;
		FQuat PreviousRotation = PreviousWorldTransform.GetRotation().GetNormalized();
		FQuat CurrentRotation = CurrentWorldTransform.GetRotation().GetNormalized();
		FQuat DeltaRotation = CurrentRotation * PreviousRotation.Inverse();
		DeltaRotation.Normalize();
		if (DeltaRotation.W < 0.0)
		{
			DeltaRotation = FQuat(-DeltaRotation.X, -DeltaRotation.Y, -DeltaRotation.Z, -DeltaRotation.W);
		}

		FVector WorldAngularVelocity = FVector::ZeroVector;
		FVector Axis = FVector::ZeroVector;
		double Angle = 0.0;
		DeltaRotation.ToAxisAndAngle(Axis, Angle);
		if (!Axis.IsNearlyZero() && FMath::IsFinite(Angle))
		{
			WorldAngularVelocity = Axis.GetSafeNormal() * (Angle / DeltaTime);
		}

		Motion.LinearVelocity = SmokeTransform.InverseTransformVectorNoScale(WorldLinearVelocity);
		Motion.AngularVelocity = SmokeTransform.InverseTransformVectorNoScale(WorldAngularVelocity);
		return Motion;
	}

	void AddSphereObstaclePrimitive(
		const FVector& SmokeLocalCenter,
		float Radius,
		const FSmokeObstacleMotion& Motion,
		TArray<FTimeThiefSmokeObstaclePrimitive>& OutPrimitives)
	{
		FTimeThiefSmokeObstaclePrimitive Primitive;
		const float SafeRadius = FMath::Max(Radius, 1.0f);
		Primitive.CenterRadius = FVector4f(SmokeLocalCenter.X, SmokeLocalCenter.Y, SmokeLocalCenter.Z, SafeRadius);
		Primitive.AxisHalfLength = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
		Primitive.ExtentsShape = FVector4f(SafeRadius, SafeRadius, SafeRadius, static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Sphere)));
		Primitive.Rotation = FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
		SetObstaclePrimitiveMotion(Primitive, Motion);
		OutPrimitives.Add(Primitive);
	}

	void AddCapsuleObstaclePrimitive(
		const FVector& SmokeLocalCenter,
		const FVector& SmokeLocalAxis,
		float Radius,
		float HalfSegment,
		const FSmokeObstacleMotion& Motion,
		TArray<FTimeThiefSmokeObstaclePrimitive>& OutPrimitives)
	{
		FTimeThiefSmokeObstaclePrimitive Primitive;
		const float SafeRadius = FMath::Max(Radius, 1.0f);
		const float SafeHalfSegment = FMath::Max(HalfSegment, 0.0f);
		const FVector SafeAxis = SmokeLocalAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		Primitive.CenterRadius = FVector4f(SmokeLocalCenter.X, SmokeLocalCenter.Y, SmokeLocalCenter.Z, SafeRadius);
		Primitive.AxisHalfLength = FVector4f(SafeAxis.X, SafeAxis.Y, SafeAxis.Z, SafeHalfSegment);
		Primitive.ExtentsShape = FVector4f(SafeRadius, SafeRadius, SafeHalfSegment + SafeRadius, static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Capsule)));
		Primitive.Rotation = FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
		SetObstaclePrimitiveMotion(Primitive, Motion);
		OutPrimitives.Add(Primitive);
	}

	void AddBoxObstaclePrimitive(
		const FVector& SmokeLocalCenter,
		const FVector& Extents,
		const FQuat& SmokeLocalRotation,
		const FSmokeObstacleMotion& Motion,
		TArray<FTimeThiefSmokeObstaclePrimitive>& OutPrimitives)
	{
		FTimeThiefSmokeObstaclePrimitive Primitive;
		const FVector SafeExtents = Extents.ComponentMax(FVector(1.0f));
		Primitive.CenterRadius = FVector4f(SmokeLocalCenter.X, SmokeLocalCenter.Y, SmokeLocalCenter.Z, SafeExtents.GetMax());
		Primitive.AxisHalfLength = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
		Primitive.ExtentsShape = FVector4f(SafeExtents.X, SafeExtents.Y, SafeExtents.Z, static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Box)));
		Primitive.Rotation = FVector4f(SmokeLocalRotation.X, SmokeLocalRotation.Y, SmokeLocalRotation.Z, SmokeLocalRotation.W);
		SetObstaclePrimitiveMotion(Primitive, Motion);
		OutPrimitives.Add(Primitive);
	}

	bool AddBodySetupObstaclePrimitives(
		const UPrimitiveComponent* PrimitiveComponent,
		const FTransform& SmokeTransform,
		const FSmokeObstacleMotion& Motion,
		TArray<FTimeThiefSmokeObstaclePrimitive>& OutPrimitives,
		int32 MaxPrimitiveCount)
	{
		UBodySetup* BodySetup = PrimitiveComponent ? const_cast<UPrimitiveComponent*>(PrimitiveComponent)->GetBodySetup() : nullptr;
		if (!BodySetup || BodySetup->AggGeom.GetElementCount() <= 0)
		{
			return false;
		}

		const int32 AddedBefore = OutPrimitives.Num();
		const FTransform ComponentTransform = PrimitiveComponent->GetComponentTransform();
		const FVector ComponentScale = ComponentTransform.GetScale3D();
		const FQuat ComponentRotation = ComponentTransform.GetRotation();
		const FQuat SmokeInverseRotation = SmokeTransform.GetRotation().Inverse();
		const FTransform ComponentToWorldNoScale(ComponentRotation, ComponentTransform.GetLocation(), FVector::OneVector);
		const auto HasCapacity = [&OutPrimitives, MaxPrimitiveCount]()
		{
			return OutPrimitives.Num() < MaxPrimitiveCount;
		};

		for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
		{
			if (!HasCapacity())
			{
				break;
			}

			const FKSphereElem ScaledSphere = SphereElem.GetFinalScaled(ComponentScale, FTransform::Identity);
			const FVector WorldCenter = ComponentToWorldNoScale.TransformPosition(ScaledSphere.Center);
			AddSphereObstaclePrimitive(SmokeTransform.InverseTransformPosition(WorldCenter), ScaledSphere.Radius, Motion, OutPrimitives);
		}

		for (const FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
		{
			if (!HasCapacity())
			{
				break;
			}

			const FKBoxElem ScaledBox = BoxElem.GetFinalScaled(ComponentScale, FTransform::Identity);
			const FVector WorldCenter = ComponentToWorldNoScale.TransformPosition(ScaledBox.Center);
			const FQuat SmokeLocalRotation = SmokeInverseRotation * ComponentRotation * ScaledBox.Rotation.Quaternion();
			AddBoxObstaclePrimitive(
				SmokeTransform.InverseTransformPosition(WorldCenter),
				FVector(ScaledBox.X, ScaledBox.Y, ScaledBox.Z) * 0.5f,
				SmokeLocalRotation,
				Motion,
				OutPrimitives);
		}

		for (const FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
		{
			if (!HasCapacity())
			{
				break;
			}

			const FKSphylElem ScaledSphyl = SphylElem.GetFinalScaled(ComponentScale, FTransform::Identity);
			const FQuat WorldRotation = ComponentRotation * ScaledSphyl.Rotation.Quaternion();
			const FVector WorldCenter = ComponentToWorldNoScale.TransformPosition(ScaledSphyl.Center);
			AddCapsuleObstaclePrimitive(
				SmokeTransform.InverseTransformPosition(WorldCenter),
				SmokeInverseRotation.RotateVector(WorldRotation.GetAxisZ()),
				ScaledSphyl.Radius,
				ScaledSphyl.Length * 0.5f,
				Motion,
				OutPrimitives);
		}

		return OutPrimitives.Num() > AddedBefore;
	}

	bool MakeObstaclePrimitive(
		const UPrimitiveComponent* PrimitiveComponent,
		const FTransform& SmokeTransform,
		const FSmokeObstacleMotion& Motion,
		FTimeThiefSmokeObstaclePrimitive& OutPrimitive)
	{
		if (!PrimitiveComponent)
		{
			return false;
		}

		const FQuat SmokeInverseRotation = SmokeTransform.GetRotation().Inverse();
		const FVector LocalCenter = SmokeTransform.InverseTransformPosition(PrimitiveComponent->Bounds.Origin);
		FVector Extents = PrimitiveComponent->Bounds.BoxExtent;
		ETimeThiefSmokeObstaclePrimitiveShape Shape = ETimeThiefSmokeObstaclePrimitiveShape::Box;
		float Radius = Extents.GetMax();
		float HalfSegment = 0.0f;
		FVector Axis = FVector::UpVector;
		FQuat LocalRotation = FQuat::Identity;

		if (const USphereComponent* SphereComponent = Cast<USphereComponent>(PrimitiveComponent))
		{
			Shape = ETimeThiefSmokeObstaclePrimitiveShape::Sphere;
			Radius = SphereComponent->GetScaledSphereRadius();
			Extents = FVector(Radius);
		}
		else if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(PrimitiveComponent))
		{
			Shape = ETimeThiefSmokeObstaclePrimitiveShape::Capsule;
			Radius = CapsuleComponent->GetScaledCapsuleRadius();
			HalfSegment = FMath::Max(CapsuleComponent->GetScaledCapsuleHalfHeight() - Radius, 0.0f);
			Axis = SmokeInverseRotation.RotateVector(CapsuleComponent->GetComponentQuat().GetAxisZ()).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
			Extents = FVector(Radius, Radius, HalfSegment + Radius);
		}
		else if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(PrimitiveComponent))
		{
			Shape = ETimeThiefSmokeObstaclePrimitiveShape::Box;
			Extents = BoxComponent->GetScaledBoxExtent();
			Radius = Extents.GetMax();
			LocalRotation = SmokeInverseRotation * BoxComponent->GetComponentQuat();
		}
		else
		{
			return false;
		}

		OutPrimitive.CenterRadius = FVector4f(LocalCenter.X, LocalCenter.Y, LocalCenter.Z, Radius);
		OutPrimitive.AxisHalfLength = FVector4f(Axis.X, Axis.Y, Axis.Z, HalfSegment);
		OutPrimitive.ExtentsShape = FVector4f(Extents.X, Extents.Y, Extents.Z, static_cast<float>(static_cast<uint8>(Shape)));
		OutPrimitive.Rotation = FVector4f(LocalRotation.X, LocalRotation.Y, LocalRotation.Z, LocalRotation.W);
		SetObstaclePrimitiveMotion(OutPrimitive, Motion);
		return true;
	}

	bool AnyObstaclePrimitiveBlocksCell(
		const TArray<FTimeThiefSmokeObstaclePrimitive>& ObstaclePrimitives,
		const FVector& LocalCenter,
		const FVector& CellHalfExtent)
	{
		const float CellRadius = CellHalfExtent.Size();
		for (const FTimeThiefSmokeObstaclePrimitive& Primitive : ObstaclePrimitives)
		{
			if (EvaluateObstaclePrimitiveSdf(Primitive, LocalCenter) <= CellRadius)
			{
				return true;
			}
		}
		return false;
	}

	FIntVector LocalPositionToCellCoord(
		const FVector& LocalPosition,
		const FVector& BoundsExtent,
		const FVector& GridOffset,
		const FIntVector& CellGrid)
	{
		const FVector Relative = LocalPosition - GridOffset;
		const FVector Alpha = (Relative / BoundsExtent) * 0.5f + FVector(0.5f);
		return FIntVector(
			FMath::FloorToInt(Alpha.X * static_cast<float>(CellGrid.X)),
			FMath::FloorToInt(Alpha.Y * static_cast<float>(CellGrid.Y)),
			FMath::FloorToInt(Alpha.Z * static_cast<float>(CellGrid.Z)));
	}

	void AddObstacleCandidateToCellBuckets(
		const int32 CandidateIndex,
		const FBox& LocalBounds,
		const FVector& BoundsExtent,
		const FVector& GridOffset,
		const FVector& QueryExtent,
		const FIntVector& CellGrid,
		TArray<TArray<int32>>& CellBuckets)
	{
		if (!LocalBounds.IsValid)
		{
			return;
		}

		const FBox ExpandedLocalBounds = LocalBounds.ExpandBy(QueryExtent);
		const FBox LocalCellRange(GridOffset - BoundsExtent, GridOffset + BoundsExtent);
		if (!ExpandedLocalBounds.Intersect(LocalCellRange))
		{
			return;
		}

		FIntVector MinCoord = LocalPositionToCellCoord(ExpandedLocalBounds.Min, BoundsExtent, GridOffset, CellGrid);
		FIntVector MaxCoord = LocalPositionToCellCoord(ExpandedLocalBounds.Max, BoundsExtent, GridOffset, CellGrid);
		MinCoord.X = FMath::Clamp(MinCoord.X, 0, CellGrid.X - 1);
		MinCoord.Y = FMath::Clamp(MinCoord.Y, 0, CellGrid.Y - 1);
		MinCoord.Z = FMath::Clamp(MinCoord.Z, 0, CellGrid.Z - 1);
		MaxCoord.X = FMath::Clamp(MaxCoord.X, 0, CellGrid.X - 1);
		MaxCoord.Y = FMath::Clamp(MaxCoord.Y, 0, CellGrid.Y - 1);
		MaxCoord.Z = FMath::Clamp(MaxCoord.Z, 0, CellGrid.Z - 1);
		for (int32 Z = MinCoord.Z; Z <= MaxCoord.Z; ++Z)
		{
			for (int32 Y = MinCoord.Y; Y <= MaxCoord.Y; ++Y)
			{
				for (int32 X = MinCoord.X; X <= MaxCoord.X; ++X)
				{
					CellBuckets[FlattenCellCoord(FIntVector(X, Y, Z), CellGrid)].Add(CandidateIndex);
				}
			}
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	int32 CountObstacleBucketReferences(const TArray<TArray<int32>>& CellBuckets, const int32 CandidateIndex)
	{
		int32 Count = 0;
		for (const TArray<int32>& Bucket : CellBuckets)
		{
			for (const int32 BucketCandidateIndex : Bucket)
			{
				if (BucketCandidateIndex == CandidateIndex)
				{
					++Count;
				}
			}
		}
		return Count;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FTimeThiefSmokeObstacleBucketAutomationTest,
		"TimeThief.Smoke.Volume.ObstacleBuckets",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FTimeThiefSmokeObstacleBucketAutomationTest::RunTest(const FString& Parameters)
	{
		const FIntVector CellGrid(8, 8, 8);
		const FVector BoundsExtent(800.0, 800.0, 800.0);
		const FVector GridOffset = FVector::ZeroVector;
		const FVector QueryExtent(20.0, 20.0, 20.0);
		TArray<TArray<int32>> CellBuckets;
		CellBuckets.SetNum(CellGrid.X * CellGrid.Y * CellGrid.Z);

		AddObstacleCandidateToCellBuckets(
			0,
			FBox(FVector(-40.0, -40.0, -40.0), FVector(40.0, 40.0, 40.0)),
			BoundsExtent,
			GridOffset,
			QueryExtent,
			CellGrid,
			CellBuckets);
		TestTrue(TEXT("Centered obstacle candidate maps to a small subset of cells"), CountObstacleBucketReferences(CellBuckets, 0) > 0);
		TestTrue(TEXT("Centered obstacle candidate does not touch every obstacle cell"), CountObstacleBucketReferences(CellBuckets, 0) < CellBuckets.Num());

		AddObstacleCandidateToCellBuckets(
			1,
			FBox(FVector(3000.0, 3000.0, 3000.0), FVector(3100.0, 3100.0, 3100.0)),
			BoundsExtent,
			GridOffset,
			QueryExtent,
			CellGrid,
			CellBuckets);
		TestEqual(TEXT("Outside obstacle candidate maps to no cells"), CountObstacleBucketReferences(CellBuckets, 1), 0);

		return true;
	}
#endif
}

ATimeThiefSmokeVolume::ATimeThiefSmokeVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SmokeBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("SmokeBounds"));
	SetRootComponent(SmokeBoundsComponent);
	SmokeBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SmokeBoundsComponent->SetGenerateOverlapEvents(false);
}

void ATimeThiefSmokeVolume::InitializeSmokeVolume(AActor* InOwnerActor, APawn* InInstigatorPawn)
{
	if (SmokeId == INDEX_NONE)
	{
		if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
		{
			SmokeId = SmokeSubsystem->AllocateSmokeId();
		}
		else
		{
			SmokeId = static_cast<int32>(GetUniqueID());
		}
	}
	SmokeAgeSeconds = 0.0f;
	ObstaclePrimitives.Reset();
	ObstacleFieldSignature = 0;
	ObstacleFieldResolution = 0;
	ObstacleFieldRevision = 0;
	bHasBuiltObstacleField = false;
	LastSmokeSpatialBounds = FBox(EForceInit::ForceInit);

	SetOwner(InOwnerActor);
	SetInstigator(InInstigatorPawn);
	if (SmokeBoundsComponent)
	{
		SmokeBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetLifeSpan(TimeThiefSmokeParameterDefaults::SmokeDuration + TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration);

	UpdateSmokeBounds();
	MarkObstacleFieldDirty();
	LastSmokeSpatialBounds = GetCurrentSmokeWorldBounds();
}

FVector ATimeThiefSmokeVolume::GetCurrentSmokeBoundsExtent() const
{
	return TimeThiefSmokeParameterDefaults::GetSmokeBoundsExtent();
}

FVector ATimeThiefSmokeVolume::GetCurrentSmokeRenderBoundsExtent() const
{
	return GetCurrentSmokeBoundsExtent() + TimeThiefSmokeParameterDefaults::GetRenderBoundsPadding();
}

FBox ATimeThiefSmokeVolume::GetCurrentSmokeWorldBounds() const
{
	const FVector BoundsExtent = GetCurrentSmokeRenderBoundsExtent();
	const FTransform SmokeTransform = GetActorTransform();
	FBox Bounds(EForceInit::ForceInit);

	for (int32 Z = -1; Z <= 1; Z += 2)
	{
		for (int32 Y = -1; Y <= 1; Y += 2)
		{
			for (int32 X = -1; X <= 1; X += 2)
			{
				const FVector LocalCorner(BoundsExtent.X * X, BoundsExtent.Y * Y, BoundsExtent.Z * Z);
				Bounds += SmokeTransform.TransformPosition(LocalCorner);
			}
		}
	}

	return Bounds;
}

void ATimeThiefSmokeVolume::FlushPendingObstacleFieldRebuild(float DeltaTime)
{
	if (!TimeThiefSmokeParameterDefaults::bUseStaticObstacleMask)
	{
		if (bHasBuiltObstacleField || ObstacleFieldSignature != 0 || !ObstaclePrimitives.IsEmpty())
		{
			bHasBuiltObstacleField = true;
			ObstaclePrimitives.Reset();
			ObstacleFieldResolution = 0;
			bHasSolidObstacleField = false;
			ObstacleFieldSignature = 0;
			++ObstacleFieldRevision;
		}
		return;
	}

	if (bObstacleFieldRebuildPending && !bHasBuiltObstacleField)
	{
		RebuildStaticObstacleField(DeltaTime);
	}
}

bool ATimeThiefSmokeVolume::IntersectTraceSegment(const FVector& SegmentStart, const FVector& SegmentEnd, FVector& OutEntryPoint, FVector& OutExitPoint) const
{
	if (!SmokeBoundsComponent)
	{
		return false;
	}

	const FTransform BoundsTransform = SmokeBoundsComponent->GetComponentTransform();
	const FVector LocalStart = BoundsTransform.InverseTransformPosition(SegmentStart);
	const FVector LocalEnd = BoundsTransform.InverseTransformPosition(SegmentEnd);
	const FVector LocalDelta = LocalEnd - LocalStart;
	const FVector Extent = GetCurrentSmokeRenderBoundsExtent();

	float TMin = 0.0f;
	float TMax = 1.0f;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float StartValue = LocalStart[Axis];
		const float DeltaValue = LocalDelta[Axis];
		const float MinValue = -Extent[Axis];
		const float MaxValue = Extent[Axis];

		if (FMath::IsNearlyZero(DeltaValue))
		{
			if (StartValue < MinValue || StartValue > MaxValue)
			{
				return false;
			}
			continue;
		}

		float NearT = (MinValue - StartValue) / DeltaValue;
		float FarT = (MaxValue - StartValue) / DeltaValue;
		if (NearT > FarT)
		{
			Swap(NearT, FarT);
		}

		TMin = FMath::Max(TMin, NearT);
		TMax = FMath::Min(TMax, FarT);
		if (TMin > TMax)
		{
			return false;
		}
	}

	OutEntryPoint = FMath::Lerp(SegmentStart, SegmentEnd, TMin);
	OutExitPoint = FMath::Lerp(SegmentStart, SegmentEnd, TMax);
	return FVector::DistSquared(OutEntryPoint, OutExitPoint) > FMath::Square(1.0f);
}

bool ATimeThiefSmokeVolume::IntersectsExplosion(const FVector& Center, float Radius) const
{
	if (!SmokeBoundsComponent)
	{
		return false;
	}

	const FTransform BoundsTransform = SmokeBoundsComponent->GetComponentTransform();
	const FVector LocalCenter = BoundsTransform.InverseTransformPosition(Center);
	const FVector Extent = GetCurrentSmokeRenderBoundsExtent();
	const FVector ClosestLocal(
		FMath::Clamp(LocalCenter.X, -Extent.X, Extent.X),
		FMath::Clamp(LocalCenter.Y, -Extent.Y, Extent.Y),
		FMath::Clamp(LocalCenter.Z, -Extent.Z, Extent.Z));
	const FVector ClosestWorld = BoundsTransform.TransformPosition(ClosestLocal);
	return FVector::DistSquared(ClosestWorld, Center) <= FMath::Square(FMath::Max(1.0f, Radius));
}

void ATimeThiefSmokeVolume::HandleBulletTrace(const FVector& EntryPoint, const FVector& ExitPoint, float Strength, int32 Seed)
{
	const FVector Segment = ExitPoint - EntryPoint;
	const float SegmentLength = Segment.Size();
	if (SegmentLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FRandomStream RandomStream(Seed ^ (SmokeId * 196613));

	const FVector Direction = Segment / SegmentLength;
	FTimeThiefSmokeInteractionEvent Event;
	Event.SmokeId = SmokeId;
	Event.Type = ESmokeInteractionType::BulletWake;
	Event.Shape = ESmokeInteractionShape::LineWake;
	Event.Position = (EntryPoint + ExitPoint) * 0.5f;
	Event.PreviousPosition = Event.Position;
	Event.Direction = Direction;
	Event.Rotation = Direction.Rotation().Quaternion();
	Event.Radius = TimeThiefSmokeParameterDefaults::BulletClearRadius * RandomStream.FRandRange(TimeThiefSmokeParameterDefaults::BulletClearRadiusRandomMin, TimeThiefSmokeParameterDefaults::BulletClearRadiusRandomMax);
	Event.Length = SegmentLength + Event.Radius * 2.0f;
	Event.Strength = FMath::Clamp(Strength * RandomStream.FRandRange(TimeThiefSmokeParameterDefaults::BulletWakeStrengthRandomMin, TimeThiefSmokeParameterDefaults::BulletWakeStrengthRandomMax), 0.0f, 1.0f);
	Event.NormalizedAge = 0.0f;
	Event.Seed = RandomStream.RandRange(1, INT32_MAX - 1);
	if (FTimeThiefSmokeTestBridge::IsActive())
	{
		FTimeThiefSmokeTestEvent TestEvent;
		TestEvent.Type = TEXT("bullet_event_generated");
		TestEvent.SmokeId = SmokeId;
		TestEvent.Entry = EntryPoint;
		TestEvent.Exit = ExitPoint;
		TestEvent.Position = Event.Position;
		TestEvent.Direction = Event.Direction;
		TestEvent.Radius = Event.Radius;
		TestEvent.Length = Event.Length;
		TestEvent.Strength = Event.Strength;
		TestEvent.Seed = Event.Seed;
		TestEvent.FrameId = GFrameCounter;
		FTimeThiefSmokeTestBridge::Emit(TestEvent);
	}

	ApplyInteractionEvent(Event);
}

void ATimeThiefSmokeVolume::HandleExplosionShock(const FVector& Center, float Radius, float Strength, int32 Seed)
{
	FTimeThiefSmokeInteractionEvent Event;
	Event.SmokeId = SmokeId;
	Event.Type = ESmokeInteractionType::ExplosionShock;
	Event.Shape = ESmokeInteractionShape::Sphere;
	Event.Position = Center;
	Event.PreviousPosition = Event.Position;
	Event.Direction = (GetActorLocation() - Center).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	Event.Rotation = FQuat::Identity;
	Event.Radius = FMath::Max(Radius, TimeThiefSmokeParameterDefaults::ExplosionShockRadius);
	Event.Length = Event.Radius;
	Event.Strength = FMath::Max(0.0f, Strength);
	Event.Extents = FVector(TimeThiefSmokeParameterDefaults::ExplosionOutwardStrength, 0.0f, 0.0f);
	Event.NormalizedAge = 0.0f;
	Event.Seed = Seed;
	if (FTimeThiefSmokeTestBridge::IsActive())
	{
		FTimeThiefSmokeTestEvent TestEvent;
		TestEvent.Type = TEXT("explosion_event_generated");
		TestEvent.SmokeId = SmokeId;
		TestEvent.Position = Event.Position;
		TestEvent.Direction = Event.Direction;
		TestEvent.Extents = Event.Extents;
		TestEvent.Radius = Event.Radius;
		TestEvent.Strength = Event.Strength;
		TestEvent.Seed = Event.Seed;
		TestEvent.FrameId = GFrameCounter;
		FTimeThiefSmokeTestBridge::Emit(TestEvent);
	}

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->AddTimedInteractionEvent(this, Event, TimeThiefSmokeParameterDefaults::ExplosionImpulseDuration);
	}
	else
	{
		ApplyInteractionEvent(Event);
	}
}

void ATimeThiefSmokeVolume::ApplyInteractionEvent(const FTimeThiefSmokeInteractionEvent& Event)
{
	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->RecordRendererEvent(Event);
	}
}

void ATimeThiefSmokeVolume::BeginPlay()
{
	Super::BeginPlay();

	if (SmokeBoundsComponent)
	{
		SmokeBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (SmokeId == INDEX_NONE)
	{
		if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
		{
			SmokeId = SmokeSubsystem->AllocateSmokeId();
		}
		else
		{
			SmokeId = static_cast<int32>(GetUniqueID());
		}
	}

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		if (FTimeThiefSmokeTestBridge::IsActive())
		{
			FTimeThiefSmokeTestEvent Event;
			Event.Type = TEXT("smoke_spawned");
			Event.SmokeId = SmokeId;
			Event.Position = GetActorLocation();
			Event.Extents = GetCurrentSmokeBoundsExtent();
			Event.FrameId = GFrameCounter;
			FTimeThiefSmokeTestBridge::Emit(Event);
		}
		SmokeSubsystem->RegisterSmokeVolume(this);
	}
	LastSmokeSpatialBounds = GetCurrentSmokeWorldBounds();
}

void ATimeThiefSmokeVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->UnregisterSmokeVolume(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ATimeThiefSmokeVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SmokeAgeSeconds += FMath::Max(0.0f, DeltaTime);
	NotifySpatialBoundsIfChanged();
}

void ATimeThiefSmokeVolume::GatherActorPushEventsFromSamples(const TArray<FTimeThiefSmokeActorPushSample>& CandidateSamples)
{
	if (!SmokeBoundsComponent || CandidateSamples.IsEmpty())
	{
		return;
	}

	TArray<FTimeThiefSmokeInteractionEvent> ActorEvents;
	const int32 MaxEvents = TimeThiefSmokeParameterDefaults::MaxActorInteractionEventsPerTick;
	if (MaxEvents > 0)
	{
		ActorEvents.Reserve(FMath::Min(CandidateSamples.Num(), MaxEvents));
	}

	const FBox SmokeWorldBounds = GetCurrentSmokeWorldBounds();
	for (const FTimeThiefSmokeActorPushSample& Sample : CandidateSamples)
	{
		UPrimitiveComponent* PrimitiveComponent = Sample.PrimitiveComponent.Get();
		AActor* OverlapOwner = PrimitiveComponent ? PrimitiveComponent->GetOwner() : nullptr;
		if (!PrimitiveComponent ||
			PrimitiveComponent == SmokeBoundsComponent ||
			!OverlapOwner ||
			OverlapOwner == this ||
			OverlapOwner->IsA<ATimeThiefSmokeVolume>() ||
			PrimitiveComponent->Mobility == EComponentMobility::Static)
		{
			continue;
		}
		if (!Sample.ComponentBounds.Intersect(SmokeWorldBounds) || Sample.Strength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FTimeThiefSmokeInteractionEvent Event;
		Event.SmokeId = SmokeId;
		Event.Type = ESmokeInteractionType::ActorPush;
		Event.Shape = Sample.Shape;
		Event.Position = Sample.Position;
		Event.PreviousPosition = Sample.PreviousPosition;
		Event.Direction = Sample.Direction;
		Event.Rotation = Sample.Rotation;
		Event.Extents = Sample.Extents;
		Event.Radius = Sample.Radius;
		Event.Length = Sample.Length;
		Event.Strength = Sample.Strength;
		Event.Speed = Sample.Speed;
		Event.NormalizedAge = 0.0f;
		Event.Seed = Sample.Seed;
		if (FTimeThiefSmokeTestBridge::IsActive())
		{
			FTimeThiefSmokeTestEvent Entered;
			Entered.Type = TEXT("actor_entered_smoke");
			Entered.SmokeId = SmokeId;
			Entered.ActorName = OverlapOwner->GetName();
			Entered.ComponentName = PrimitiveComponent->GetName();
			Entered.Position = Sample.Position;
			Entered.PreviousPosition = Sample.PreviousPosition;
			Entered.Speed = Sample.Speed;
			Entered.FrameId = GFrameCounter;
			FTimeThiefSmokeTestBridge::Emit(Entered);

			FTimeThiefSmokeTestEvent Generated = Entered;
			Generated.Type = TEXT("actor_push_event_generated");
			switch (Sample.Shape)
			{
			case ESmokeInteractionShape::Sphere: Generated.Shape = TEXT("sphere"); break;
			case ESmokeInteractionShape::Capsule: Generated.Shape = TEXT("capsule"); break;
			case ESmokeInteractionShape::Box: Generated.Shape = TEXT("box"); break;
			default: Generated.Shape = TEXT("unknown"); break;
			}
			Generated.Direction = Sample.Direction;
			Generated.Extents = Sample.Extents;
			Generated.Radius = Sample.Radius;
			Generated.Length = Sample.Length;
			Generated.Strength = Sample.Strength;
			Generated.Seed = Sample.Seed;
			FTimeThiefSmokeTestBridge::Emit(Generated);
		}

		if (MaxEvents <= 0)
		{
			if (FTimeThiefSmokeTestBridge::IsActive())
			{
				FTimeThiefSmokeTestEvent Rejected;
				Rejected.Type = TEXT("actor_push_event_rejected");
				Rejected.SmokeId = SmokeId;
				Rejected.ActorName = OverlapOwner->GetName();
				Rejected.ComponentName = PrimitiveComponent->GetName();
				Rejected.FrameId = GFrameCounter;
				FTimeThiefSmokeTestBridge::Emit(Rejected);
			}
			continue;
		}

		if (ActorEvents.Num() < MaxEvents)
		{
			ActorEvents.Add(Event);
			continue;
		}

		int32 WeakestEventIndex = INDEX_NONE;
		float WeakestStrength = TNumericLimits<float>::Max();
		for (int32 ExistingEventIndex = 0; ExistingEventIndex < ActorEvents.Num(); ++ExistingEventIndex)
		{
			if (ActorEvents[ExistingEventIndex].Strength < WeakestStrength)
			{
				WeakestStrength = ActorEvents[ExistingEventIndex].Strength;
				WeakestEventIndex = ExistingEventIndex;
			}
		}

		if (WeakestEventIndex != INDEX_NONE && Event.Strength > WeakestStrength)
		{
			ActorEvents[WeakestEventIndex] = Event;
		}
		else if (FTimeThiefSmokeTestBridge::IsActive())
		{
			FTimeThiefSmokeTestEvent Rejected;
			Rejected.Type = TEXT("actor_push_event_rejected");
			Rejected.SmokeId = SmokeId;
			Rejected.ActorName = OverlapOwner->GetName();
			Rejected.ComponentName = PrimitiveComponent->GetName();
			Rejected.Strength = Event.Strength;
			Rejected.FrameId = GFrameCounter;
			FTimeThiefSmokeTestBridge::Emit(Rejected);
		}
	}

	ActorEvents.Sort([](const FTimeThiefSmokeInteractionEvent& A, const FTimeThiefSmokeInteractionEvent& B)
	{
		return A.Strength > B.Strength;
	});

	for (int32 EventIndex = 0; EventIndex < ActorEvents.Num(); ++EventIndex)
	{
		const FTimeThiefSmokeInteractionEvent& Event = ActorEvents[EventIndex];
		ApplyInteractionEvent(Event);
	}
}

void ATimeThiefSmokeVolume::MarkObstacleFieldDirty()
{
	bObstacleFieldRebuildPending = true;
}

void ATimeThiefSmokeVolume::RebuildStaticObstacleField(float DeltaTime)
{
	bObstacleFieldRebuildPending = false;

	UWorld* World = GetWorld();
	if (!World || !TimeThiefSmokeParameterDefaults::bUseStaticObstacleMask)
	{
		const bool bHadObstacleField = bHasBuiltObstacleField || ObstacleFieldSignature != 0 || !ObstaclePrimitives.IsEmpty();
		ObstaclePrimitives.Reset();
		ObstacleFieldResolution = 0;
		bHasSolidObstacleField = false;
		ObstacleFieldSignature = 0;
		bHasBuiltObstacleField = true;
		if (bHadObstacleField)
		{
			++ObstacleFieldRevision;
		}
		return;
	}

	const int32 Resolution = TimeThiefSmokeParameterDefaults::ObstacleMaskResolution;
	ObstacleFieldResolution = Resolution;

	const FVector NaturalBoundsExtent = GetCurrentSmokeBoundsExtent();
	const FVector BoundsExtent = GetCurrentSmokeRenderBoundsExtent();
	const FVector CellHalfExtent = BoundsExtent / static_cast<float>(Resolution);
	const FVector ObstacleQueryExtent = TimeThiefSmokeVolume::MakeObstacleMaskQueryExtent(CellHalfExtent, TimeThiefSmokeParameterDefaults::ObstacleMaskInflation);
	const FTransform SmokeTransform = GetActorTransform();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefSmokeObstacleMask), false);
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}
	if (APawn* InstigatorPawn = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorPawn);
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	const FCollisionShape BoundsShape = FCollisionShape::MakeBox(BoundsExtent + ObstacleQueryExtent);
	World->OverlapMultiByObjectType(
		Overlaps,
		SmokeTransform.GetLocation(),
		SmokeTransform.GetRotation(),
		ObjectQueryParams,
		BoundsShape,
		QueryParams);

	TSet<UPrimitiveComponent*> AddedCandidates;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> StaticObstacleCandidates;
	StaticObstacleCandidates.Reserve(Overlaps.Num());
	TimeThiefSmokeVolume::AddSmokeObstacleCandidates(Overlaps, AddedCandidates, StaticObstacleCandidates);

	TArray<TWeakObjectPtr<UPrimitiveComponent>> HardStaticCandidates;
	HardStaticCandidates.Reserve(StaticObstacleCandidates.Num());
	for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : StaticObstacleCandidates)
	{
		UPrimitiveComponent* CandidateComponent = Candidate.Get();
		if (CandidateComponent &&
			TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(CandidateComponent, this) &&
			TimeThiefSmokeVolume::HasSupportedSmokeObstacleGeometry(CandidateComponent))
		{
			HardStaticCandidates.Add(Candidate);
		}
	}

	HardStaticCandidates.Sort([](const TWeakObjectPtr<UPrimitiveComponent>& Left, const TWeakObjectPtr<UPrimitiveComponent>& Right)
	{
		const UPrimitiveComponent* LeftComponent = Left.Get();
		const UPrimitiveComponent* RightComponent = Right.Get();
		const uint32 LeftId = LeftComponent ? LeftComponent->GetUniqueID() : 0u;
		const uint32 RightId = RightComponent ? RightComponent->GetUniqueID() : 0u;
		return LeftId < RightId;
	});

	const uint64 StaticObstacleCacheKey = TimeThiefSmokeVolume::MakeObstacleMaskCacheKey(
		World,
		SmokeTransform,
		NaturalBoundsExtent,
		BoundsExtent,
		Resolution,
		HardStaticCandidates);

	uint64 CurrentObstacleFieldSignature = 0xcbf29ce484222325ull;
	TimeThiefSmokeVolume::MixObstacleMaskHash(CurrentObstacleFieldSignature, StaticObstacleCacheKey);

	ObstacleFieldSignature = CurrentObstacleFieldSignature;
	ObstaclePrimitives.Reset();
	ObstacleFieldResolution = Resolution;
	bHasSolidObstacleField = false;

	auto AddCandidatePrimitives = [&SmokeTransform](const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Candidates, TArray<FTimeThiefSmokeObstaclePrimitive>& OutPrimitives)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : Candidates)
		{
			if (OutPrimitives.Num() >= TimeThiefSmokeParameterDefaults::MaxObstaclePrimitives)
			{
				break;
			}

			UPrimitiveComponent* CandidateComponent = Candidate.Get();
			const TimeThiefSmokeVolume::FSmokeObstacleMotion Motion;
			if (TimeThiefSmokeVolume::AddBodySetupObstaclePrimitives(
				CandidateComponent,
				SmokeTransform,
				Motion,
				OutPrimitives,
				TimeThiefSmokeParameterDefaults::MaxObstaclePrimitives))
			{
				continue;
			}

			FTimeThiefSmokeObstaclePrimitive Primitive;
			if (TimeThiefSmokeVolume::MakeObstaclePrimitive(CandidateComponent, SmokeTransform, Motion, Primitive))
			{
				OutPrimitives.Add(Primitive);
			}
		}
	};

	ObstaclePrimitives.Reserve(TimeThiefSmokeParameterDefaults::MaxObstaclePrimitives);
	AddCandidatePrimitives(HardStaticCandidates, ObstaclePrimitives);
	bHasSolidObstacleField = !ObstaclePrimitives.IsEmpty();

	bHasBuiltObstacleField = true;
	++ObstacleFieldRevision;
}

void ATimeThiefSmokeVolume::UpdateSmokeBounds()
{
	if (SmokeBoundsComponent)
	{
		SmokeBoundsComponent->SetBoxExtent(GetCurrentSmokeBoundsExtent(), true);
	}
}

void ATimeThiefSmokeVolume::NotifySpatialBoundsIfChanged()
{
	const FBox CurrentBounds = GetCurrentSmokeWorldBounds();
	if (!CurrentBounds.IsValid)
	{
		return;
	}

	if (!LastSmokeSpatialBounds.IsValid ||
		!LastSmokeSpatialBounds.Min.Equals(CurrentBounds.Min, 0.1) ||
		!LastSmokeSpatialBounds.Max.Equals(CurrentBounds.Max, 0.1))
	{
		LastSmokeSpatialBounds = CurrentBounds;
		if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
		{
			SmokeSubsystem->NotifySmokeVolumeBoundsChanged(this);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTimeThiefSmokeObstacleFieldAutomationTest,
	"TimeThief.Smoke.ObstacleField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTimeThiefSmokeObstacleFieldAutomationTest::RunTest(const FString& Parameters)
{
	FTimeThiefSmokeObstaclePrimitive SpherePrimitive;
	SpherePrimitive.CenterRadius = FVector4f(0.0f, 0.0f, 0.0f, 100.0f);
	SpherePrimitive.ExtentsShape = FVector4f(100.0f, 100.0f, 100.0f, static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Sphere)));
	TestTrue(TEXT("Sphere SDF is negative inside solid"), TimeThiefSmokeVolume::EvaluateObstaclePrimitiveSdf(SpherePrimitive, FVector::ZeroVector) < 0.0f);
	TestTrue(TEXT("Sphere SDF is positive outside solid"), TimeThiefSmokeVolume::EvaluateObstaclePrimitiveSdf(SpherePrimitive, FVector(150.0f, 0.0f, 0.0f)) > 0.0f);

	TArray<FTimeThiefSmokeObstaclePrimitive> Primitives;
	Primitives.Add(SpherePrimitive);
	TestTrue(TEXT("Cell intersecting SDF surface is blocked"), TimeThiefSmokeVolume::AnyObstaclePrimitiveBlocksCell(Primitives, FVector(120.0f, 0.0f, 0.0f), FVector(25.0f)));
	TestFalse(TEXT("Cell outside SDF surface remains open"), TimeThiefSmokeVolume::AnyObstaclePrimitiveBlocksCell(Primitives, FVector(200.0f, 0.0f, 0.0f), FVector(10.0f)));

	AActor* OwnerActor = NewObject<AActor>();
	UBoxComponent* BoxComponent = NewObject<UBoxComponent>(OwnerActor);
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
	TestFalse(TEXT("Overlap-only component is not a smoke obstacle"), TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(BoxComponent, nullptr));

	BoxComponent->SetCollisionObjectType(ECC_WorldStatic);
	BoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TestTrue(TEXT("WorldStatic visibility blocker is a smoke obstacle"), TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(BoxComponent, nullptr));

	BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	TestFalse(TEXT("WorldDynamic visibility-only blocker is not a smoke obstacle"), TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(BoxComponent, nullptr));
	BoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);
	BoxComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	TestTrue(TEXT("WorldDynamic blocker is a smoke obstacle"), TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(BoxComponent, nullptr));

	BoxComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
	BoxComponent->ComponentTags.Add(TimeThiefSmokeVolume::BlockProjectileTag);
	TestTrue(TEXT("BlockProjectile tag is a smoke obstacle"), TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(BoxComponent, nullptr));

	BoxComponent->ComponentTags.Add(TimeThiefSmokeVolume::IgnoreTag);
	TestFalse(TEXT("Ignore tag excludes projectile blocker"), TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(BoxComponent, nullptr));

	AActor* ServerActor = NewObject<AActor>();
	ServerActor->Tags.Add(TimeThiefSmokeVolume::ServerCollisionTag);
	UBoxComponent* ServerBoxComponent = NewObject<UBoxComponent>(ServerActor);
	ServerBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ServerBoxComponent->SetCollisionResponseToAllChannels(ECR_Block);
	ServerBoxComponent->ComponentTags.Add(TimeThiefSmokeVolume::BlockMovementTag);
	TestFalse(TEXT("Server BlockMovement-only shape is not a smoke obstacle"), TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(ServerBoxComponent, nullptr));
	ServerBoxComponent->ComponentTags.Reset();
	ServerBoxComponent->ComponentTags.Add(TimeThiefSmokeVolume::BlockProjectileTag);
	TestTrue(TEXT("Server BlockProjectile shape is a smoke obstacle"), TimeThiefSmokeVolume::IsHardSmokeObstacleComponent(ServerBoxComponent, nullptr));

	UStaticMeshComponent* BoundsOnlyComponent = NewObject<UStaticMeshComponent>(OwnerActor);
	BoundsOnlyComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoundsOnlyComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	FTimeThiefSmokeObstaclePrimitive BoundsOnlyPrimitive;
	TestFalse(
		TEXT("Bounds-only fallback mesh does not create an obstacle primitive"),
		TimeThiefSmokeVolume::MakeObstaclePrimitive(BoundsOnlyComponent, FTransform::Identity, TimeThiefSmokeVolume::FSmokeObstacleMotion(), BoundsOnlyPrimitive));

	const FTransform PreviousObstacleTransform(FQuat::Identity, FVector::ZeroVector, FVector::OneVector);
	const FTransform CurrentObstacleTransform(FQuat(FVector::UpVector, UE_PI * 0.5), FVector(100.0, 0.0, 0.0), FVector::OneVector);
	const TimeThiefSmokeVolume::FSmokeObstacleMotion ObstacleMotion = TimeThiefSmokeVolume::MakeSmokeLocalObstacleMotion(
		PreviousObstacleTransform,
		CurrentObstacleTransform,
		FTransform::Identity,
		0.5f);
	TestTrue(TEXT("Dynamic obstacle linear velocity uses transform displacement"), FMath::IsNearlyEqual(ObstacleMotion.LinearVelocity.X, 200.0, 0.1));
	TestTrue(TEXT("Dynamic obstacle angular velocity uses transform rotation"), FMath::IsNearlyEqual(ObstacleMotion.AngularVelocity.Z, UE_PI, 0.01));

	BoxComponent->ComponentTags.Reset();
	BoxComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
	BoxComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	BoxComponent->SetBoxExtent(FVector(50.0));
	BoxComponent->SetMobility(EComponentMobility::Movable);
	BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
	TestFalse(TEXT("Movable WorldDynamic component is not a static obstacle cache candidate"), TimeThiefSmokeVolume::IsStaticSmokeObstacleComponent(BoxComponent));
	BoxComponent->SetMobility(EComponentMobility::Static);
	TestTrue(TEXT("Static mobility component is a static obstacle cache candidate"), TimeThiefSmokeVolume::IsStaticSmokeObstacleComponent(BoxComponent));
	BoxComponent->SetMobility(EComponentMobility::Movable);
	BoxComponent->SetCollisionObjectType(ECC_WorldStatic);
	TestTrue(TEXT("WorldStatic object type is a static obstacle cache candidate"), TimeThiefSmokeVolume::IsStaticSmokeObstacleComponent(BoxComponent));
	BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);

	TArray<TWeakObjectPtr<UPrimitiveComponent>> HashCandidates;
	HashCandidates.Add(BoxComponent);
	const uint64 InitialObstacleKey = TimeThiefSmokeVolume::MakeObstacleMaskCacheKey(
		nullptr,
		FTransform::Identity,
		FVector(800.0),
		FVector(1200.0),
		TimeThiefSmokeParameterDefaults::ObstacleMaskResolution,
		HashCandidates);
	BoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	const uint64 VisibilityChangedObstacleKey = TimeThiefSmokeVolume::MakeObstacleMaskCacheKey(
		nullptr,
		FTransform::Identity,
		FVector(800.0),
		FVector(1200.0),
		TimeThiefSmokeParameterDefaults::ObstacleMaskResolution,
		HashCandidates);
	TestNotEqual(TEXT("Obstacle cache key changes when visibility blocking changes"), VisibilityChangedObstacleKey, InitialObstacleKey);
	BoxComponent->SetRelativeLocation(FVector(25.0, 0.0, 0.0));
	BoxComponent->UpdateBounds();
	const uint64 MovedObstacleKey = TimeThiefSmokeVolume::MakeObstacleMaskCacheKey(
		nullptr,
		FTransform::Identity,
		FVector(800.0),
		FVector(1200.0),
		TimeThiefSmokeParameterDefaults::ObstacleMaskResolution,
		HashCandidates);
	TestNotEqual(TEXT("Obstacle cache key changes when a dynamic obstacle transform changes"), MovedObstacleKey, InitialObstacleKey);

	return true;
}

#endif
