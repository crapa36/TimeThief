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
#include "Weapon/TimeThiefRocketProjectile.h"
#include "Weapon/TimeThiefThrowableProjectile.h"
#include "WorldCollision.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace TimeThiefSmokeVolume
{
	int32 GNextSmokeId = 1;
	const FName ServerCollisionTag(TEXT("ServerCollision"));
	const FName BlockProjectileTag(TEXT("BlockProjectile"));
	const FName BlockMovementTag(TEXT("BlockMovement"));
	const FName IgnoreTag(TEXT("Ignore"));

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

	bool IsSmokeProjectileObstacleComponent(const UPrimitiveComponent* PrimitiveComponent, const ATimeThiefSmokeVolume* SmokeVolume)
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
		const bool bHasMovementTag = PrimitiveComponent->ComponentHasTag(BlockMovementTag);
		const bool bIsServerCollisionShape = IsServerCollisionShape(PrimitiveComponent);
		if (bHasProjectileTag || (bIsServerCollisionShape && !bHasMovementTag && !bHasProjectileTag))
		{
			return true;
		}

		return PrimitiveComponent->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block;
	}

	void AddSmokeObstacleCandidates(
		const TArray<FOverlapResult>& Overlaps,
		const ATimeThiefSmokeVolume* SmokeVolume,
		TSet<UPrimitiveComponent*>& AddedCandidates,
		TArray<TWeakObjectPtr<UPrimitiveComponent>>& OutCandidates)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
			if (IsSmokeProjectileObstacleComponent(PrimitiveComponent, SmokeVolume) && !AddedCandidates.Contains(PrimitiveComponent))
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
				if (IsSmokeProjectileObstacleComponent(PrimitiveComponent, SmokeVolume) && !AddedCandidates.Contains(PrimitiveComponent))
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

		const FCollisionShape BoundsShape = FCollisionShape::MakeBox(BoundsExtent + QueryExtent);
		World->OverlapMultiByObjectType(
			Overlaps,
			SmokeTransform.GetLocation(),
			SmokeTransform.GetRotation(),
			ObjectQueryParams,
			BoundsShape,
			QueryParams);
		AddSmokeObstacleCandidates(Overlaps, SmokeVolume, AddedCandidates, OutCandidates);
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

	FBox MakeLocalBoundsFromWorldAabb(const FBox& WorldBounds, const FTransform& SmokeTransform)
	{
		FBox LocalBounds(EForceInit::ForceInit);
		for (int32 Z = 0; Z <= 1; ++Z)
		{
			for (int32 Y = 0; Y <= 1; ++Y)
			{
				for (int32 X = 0; X <= 1; ++X)
				{
					const FVector WorldCorner(
						X == 0 ? WorldBounds.Min.X : WorldBounds.Max.X,
						Y == 0 ? WorldBounds.Min.Y : WorldBounds.Max.Y,
						Z == 0 ? WorldBounds.Min.Z : WorldBounds.Max.Z);
					LocalBounds += SmokeTransform.InverseTransformPosition(WorldCorner);
				}
			}
		}
		return LocalBounds;
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

	void AddSphereObstaclePrimitive(
		const FVector& SmokeLocalCenter,
		float Radius,
		TArray<FTimeThiefSmokeObstaclePrimitive>& OutPrimitives)
	{
		FTimeThiefSmokeObstaclePrimitive Primitive;
		const float SafeRadius = FMath::Max(Radius, 1.0f);
		Primitive.CenterRadius = FVector4f(SmokeLocalCenter.X, SmokeLocalCenter.Y, SmokeLocalCenter.Z, SafeRadius);
		Primitive.AxisHalfLength = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
		Primitive.ExtentsShape = FVector4f(SafeRadius, SafeRadius, SafeRadius, static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Sphere)));
		Primitive.Rotation = FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
		OutPrimitives.Add(Primitive);
	}

	void AddCapsuleObstaclePrimitive(
		const FVector& SmokeLocalCenter,
		const FVector& SmokeLocalAxis,
		float Radius,
		float HalfSegment,
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
		OutPrimitives.Add(Primitive);
	}

	void AddBoxObstaclePrimitive(
		const FVector& SmokeLocalCenter,
		const FVector& Extents,
		const FQuat& SmokeLocalRotation,
		TArray<FTimeThiefSmokeObstaclePrimitive>& OutPrimitives)
	{
		FTimeThiefSmokeObstaclePrimitive Primitive;
		const FVector SafeExtents = Extents.ComponentMax(FVector(1.0f));
		Primitive.CenterRadius = FVector4f(SmokeLocalCenter.X, SmokeLocalCenter.Y, SmokeLocalCenter.Z, SafeExtents.GetMax());
		Primitive.AxisHalfLength = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
		Primitive.ExtentsShape = FVector4f(SafeExtents.X, SafeExtents.Y, SafeExtents.Z, static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Box)));
		Primitive.Rotation = FVector4f(SmokeLocalRotation.X, SmokeLocalRotation.Y, SmokeLocalRotation.Z, SmokeLocalRotation.W);
		OutPrimitives.Add(Primitive);
	}

	bool AddLocalAabbObstaclePrimitive(
		const FBox& LocalBounds,
		TArray<FTimeThiefSmokeObstaclePrimitive>& OutPrimitives)
	{
		if (!LocalBounds.IsValid)
		{
			return false;
		}

		const FVector LocalBoundsCenter = LocalBounds.GetCenter();
		const FVector Extents = LocalBounds.GetExtent().ComponentMax(FVector(1.0f));
		FTimeThiefSmokeObstaclePrimitive Primitive;
		Primitive.CenterRadius = FVector4f(LocalBoundsCenter.X, LocalBoundsCenter.Y, LocalBoundsCenter.Z, Extents.GetMax());
		Primitive.AxisHalfLength = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
		Primitive.ExtentsShape = FVector4f(Extents.X, Extents.Y, Extents.Z, static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Aabb)));
		Primitive.Rotation = FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
		OutPrimitives.Add(Primitive);
		return true;
	}

	bool AddBodySetupObstaclePrimitives(
		const UPrimitiveComponent* PrimitiveComponent,
		const FTransform& SmokeTransform,
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
			AddSphereObstaclePrimitive(SmokeTransform.InverseTransformPosition(WorldCenter), ScaledSphere.Radius, OutPrimitives);
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
				OutPrimitives);
		}

		for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
		{
			if (!HasCapacity())
			{
				break;
			}

			const FBox ScaledLocalBounds = ConvexElem.CalcAABB(FTransform::Identity, ComponentScale);
			FBox SmokeLocalBounds(EForceInit::ForceInit);
			for (int32 Z = 0; Z <= 1; ++Z)
			{
				for (int32 Y = 0; Y <= 1; ++Y)
				{
					for (int32 X = 0; X <= 1; ++X)
					{
						const FVector ComponentLocalCorner(
							X == 0 ? ScaledLocalBounds.Min.X : ScaledLocalBounds.Max.X,
							Y == 0 ? ScaledLocalBounds.Min.Y : ScaledLocalBounds.Max.Y,
							Z == 0 ? ScaledLocalBounds.Min.Z : ScaledLocalBounds.Max.Z);
						SmokeLocalBounds += SmokeTransform.InverseTransformPosition(ComponentToWorldNoScale.TransformPosition(ComponentLocalCorner));
					}
				}
			}
			AddLocalAabbObstaclePrimitive(SmokeLocalBounds, OutPrimitives);
		}

		return OutPrimitives.Num() > AddedBefore;
	}

	bool MakeObstaclePrimitive(
		const UPrimitiveComponent* PrimitiveComponent,
		const FTransform& SmokeTransform,
		FTimeThiefSmokeObstaclePrimitive& OutPrimitive)
	{
		if (!PrimitiveComponent)
		{
			return false;
		}

		const FQuat SmokeInverseRotation = SmokeTransform.GetRotation().Inverse();
		const FVector LocalCenter = SmokeTransform.InverseTransformPosition(PrimitiveComponent->Bounds.Origin);
		FVector Extents = PrimitiveComponent->Bounds.BoxExtent;
		ETimeThiefSmokeObstaclePrimitiveShape Shape = ETimeThiefSmokeObstaclePrimitiveShape::Aabb;
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
			const FBox LocalBounds = MakeLocalBoundsFromWorldAabb(PrimitiveComponent->Bounds.GetBox(), SmokeTransform);
			if (!LocalBounds.IsValid)
			{
				return false;
			}
			Extents = LocalBounds.GetExtent();
			Radius = Extents.GetMax();
			const FVector LocalBoundsCenter = LocalBounds.GetCenter();
			OutPrimitive.CenterRadius = FVector4f(LocalBoundsCenter.X, LocalBoundsCenter.Y, LocalBoundsCenter.Z, Radius);
			OutPrimitive.AxisHalfLength = FVector4f(0.0f, 0.0f, 1.0f, 0.0f);
			OutPrimitive.ExtentsShape = FVector4f(Extents.X, Extents.Y, Extents.Z, static_cast<float>(static_cast<uint8>(ETimeThiefSmokeObstaclePrimitiveShape::Aabb)));
			OutPrimitive.Rotation = FVector4f(0.0f, 0.0f, 0.0f, 1.0f);
			return true;
		}

		OutPrimitive.CenterRadius = FVector4f(LocalCenter.X, LocalCenter.Y, LocalCenter.Z, Radius);
		OutPrimitive.AxisHalfLength = FVector4f(Axis.X, Axis.Y, Axis.Z, HalfSegment);
		OutPrimitive.ExtentsShape = FVector4f(Extents.X, Extents.Y, Extents.Z, static_cast<float>(static_cast<uint8>(Shape)));
		OutPrimitive.Rotation = FVector4f(LocalRotation.X, LocalRotation.Y, LocalRotation.Z, LocalRotation.W);
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
		const FVector& ClusterOffset,
		const FIntVector& CellGrid)
	{
		const FVector Relative = LocalPosition - ClusterOffset;
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
		const FVector& ClusterOffset,
		const FVector& QueryExtent,
		const FIntVector& CellGrid,
		TArray<TArray<int32>>& CellBuckets)
	{
		if (!LocalBounds.IsValid)
		{
			return;
		}

		const FBox ExpandedLocalBounds = LocalBounds.ExpandBy(QueryExtent);
		const FBox LocalCellRange(ClusterOffset - BoundsExtent, ClusterOffset + BoundsExtent);
		if (!ExpandedLocalBounds.Intersect(LocalCellRange))
		{
			return;
		}

		FIntVector MinCoord = LocalPositionToCellCoord(ExpandedLocalBounds.Min, BoundsExtent, ClusterOffset, CellGrid);
		FIntVector MaxCoord = LocalPositionToCellCoord(ExpandedLocalBounds.Max, BoundsExtent, ClusterOffset, CellGrid);
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
		const FVector ClusterOffset = FVector::ZeroVector;
		const FVector QueryExtent(20.0, 20.0, 20.0);
		TArray<TArray<int32>> CellBuckets;
		CellBuckets.SetNum(CellGrid.X * CellGrid.Y * CellGrid.Z);

		AddObstacleCandidateToCellBuckets(
			0,
			FBox(FVector(-40.0, -40.0, -40.0), FVector(40.0, 40.0, 40.0)),
			BoundsExtent,
			ClusterOffset,
			QueryExtent,
			CellGrid,
			CellBuckets);
		TestTrue(TEXT("Centered obstacle candidate maps to a small subset of cells"), CountObstacleBucketReferences(CellBuckets, 0) > 0);
		TestTrue(TEXT("Centered obstacle candidate does not touch every obstacle cell"), CountObstacleBucketReferences(CellBuckets, 0) < CellBuckets.Num());

		AddObstacleCandidateToCellBuckets(
			1,
			FBox(FVector(3000.0, 3000.0, 3000.0), FVector(3100.0, 3100.0, 3100.0)),
			BoundsExtent,
			ClusterOffset,
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
	SmokeBoundsComponent->SetCollisionObjectType(ECC_WorldDynamic);
	SmokeBoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SmokeBoundsComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SmokeBoundsComponent->SetGenerateOverlapEvents(false);
}

void ATimeThiefSmokeVolume::InitializeSmokeVolume(AActor* InOwnerActor, APawn* InInstigatorPawn)
{
	if (SmokeId == INDEX_NONE)
	{
		SmokeId = TimeThiefSmokeVolume::GNextSmokeId++;
	}
	SmokeAgeSeconds = 0.0f;
	PreviousComponentLocations.Reset();
	ActorWarpDensityAccumulations.Reset();

	SetOwner(InOwnerActor);
	SetInstigator(InInstigatorPawn);
	if (SmokeBoundsComponent)
	{
		SmokeBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetLifeSpan(TimeThiefSmokeParameterDefaults::SmokeDuration + TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration);

	UpdateSmokeBounds();
	RebuildStaticObstacleField();
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

void ATimeThiefSmokeVolume::FlushPendingObstacleFieldRebuild()
{
	if (bObstacleFieldRebuildPending)
	{
		RebuildStaticObstacleField();
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
		SmokeId = TimeThiefSmokeVolume::GNextSmokeId++;
	}

	if (UTimeThiefSmokeWorldSubsystem* SmokeSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTimeThiefSmokeWorldSubsystem>() : nullptr)
	{
		SmokeSubsystem->RegisterSmokeVolume(this);
	}
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
	FlushPendingObstacleFieldRebuild();
	GatherActorPushEvents(DeltaTime);
}

void ATimeThiefSmokeVolume::GatherActorPushEvents(float DeltaTime)
{
	if (!GetWorld() || !SmokeBoundsComponent || DeltaTime <= 0.0f)
	{
		return;
	}

	ActorInteractionAccumulator += DeltaTime;
	if (ActorInteractionAccumulator < (1.0f / TimeThiefSmokeParameterDefaults::ActorInteractionHz))
	{
		return;
	}

	const float SampleDeltaTime = ActorInteractionAccumulator;
	ActorInteractionAccumulator = 0.0f;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefSmokeActorOverlap), false);
	QueryParams.AddIgnoredActor(this);
	const FVector ComponentScale = SmokeBoundsComponent->GetComponentScale();
	const FVector AbsComponentScale(FMath::Abs(ComponentScale.X), FMath::Abs(ComponentScale.Y), FMath::Abs(ComponentScale.Z));
	const FVector QueryExtent = GetCurrentSmokeRenderBoundsExtent() * AbsComponentScale;

	TArray<FOverlapResult> Overlaps;
	const bool bAnyOverlap = GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		SmokeBoundsComponent->GetComponentLocation(),
		SmokeBoundsComponent->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(QueryExtent),
		QueryParams);

	if (!bAnyOverlap)
	{
		PreviousComponentLocations.Reset();
		ActorWarpDensityAccumulations.Reset();
		return;
	}

	TSet<TWeakObjectPtr<UPrimitiveComponent>> CurrentDynamicComponents;
	TArray<FTimeThiefSmokeInteractionEvent> ActorEvents;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* PrimitiveComponent = Overlap.GetComponent();
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

		CurrentDynamicComponents.Add(PrimitiveComponent);

		FTimeThiefSmokeInteractionEvent Event;
		MakeActorPushEvent(PrimitiveComponent, SampleDeltaTime, Event);
		if (Event.Strength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		ActorEvents.Add(Event);
	}

	for (auto It = PreviousComponentLocations.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<UPrimitiveComponent> Component = It.Key();
		if (!Component.IsValid() || !CurrentDynamicComponents.Contains(Component))
		{
			ActorWarpDensityAccumulations.Remove(Component);
			It.RemoveCurrent();
		}
	}

	ActorEvents.Sort([](const FTimeThiefSmokeInteractionEvent& A, const FTimeThiefSmokeInteractionEvent& B)
	{
		return A.Strength > B.Strength;
	});

	const int32 MaxEvents = TimeThiefSmokeParameterDefaults::MaxActorInteractionEventsPerTick;
	for (int32 EventIndex = 0; EventIndex < ActorEvents.Num() && EventIndex < MaxEvents; ++EventIndex)
	{
		const FTimeThiefSmokeInteractionEvent& Event = ActorEvents[EventIndex];
		ApplyInteractionEvent(Event);
	}
}

void ATimeThiefSmokeVolume::MakeActorPushEvent(UPrimitiveComponent* PrimitiveComponent, float DeltaTime, FTimeThiefSmokeInteractionEvent& OutEvent)
{
	OutEvent = FTimeThiefSmokeInteractionEvent();
	OutEvent.Strength = 0.0f;

	if (!PrimitiveComponent)
	{
		return;
	}

	FVector PreviousComponentLocation = PrimitiveComponent->GetComponentLocation();
	const FVector Velocity = ResolveComponentVelocity(PrimitiveComponent, DeltaTime, PreviousComponentLocation);
	const float Speed = Velocity.Size();
	const TWeakObjectPtr<UPrimitiveComponent> ComponentKey(PrimitiveComponent);
	float& WarpAccumulation = ActorWarpDensityAccumulations.FindOrAdd(ComponentKey);
	WarpAccumulation *= FMath::Exp(-DeltaTime / TimeThiefSmokeParameterDefaults::ActorWarpAccumulationDecaySeconds);

	OutEvent.SmokeId = SmokeId;
	OutEvent.Type = ESmokeInteractionType::ActorPush;
	const FVector CurrentBoundsOrigin = PrimitiveComponent->Bounds.Origin;
	const FVector CurrentComponentLocation = PrimitiveComponent->GetComponentLocation();
	const FVector PreviousBoundsOrigin = PreviousComponentLocation + (CurrentBoundsOrigin - CurrentComponentLocation);
	OutEvent.Position = CurrentBoundsOrigin;
	OutEvent.PreviousPosition = PreviousBoundsOrigin;
	OutEvent.Direction = Velocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	OutEvent.Rotation = PrimitiveComponent->GetComponentQuat();
	OutEvent.Speed = Speed;
	OutEvent.NormalizedAge = 0.0f;
	OutEvent.Seed = GetTypeHash(PrimitiveComponent);
	OutEvent.Shape = ResolvePrimitiveShape(PrimitiveComponent, OutEvent);

	const float CurrentDensity = EstimateWarpDensityAtWorldPosition(CurrentBoundsOrigin);
	const float PreviousDensity = EstimateWarpDensityAtWorldPosition(PreviousBoundsOrigin);
	const float MidDensity = EstimateWarpDensityAtWorldPosition((CurrentBoundsOrigin + PreviousBoundsOrigin) * 0.5);
	float DensitySum = CurrentDensity + PreviousDensity + MidDensity;
	float MaxDensity = FMath::Max3(CurrentDensity, PreviousDensity, MidDensity);
	int32 DensitySampleCount = 3;
	const FVector ProbeExtent = PrimitiveComponent->Bounds.BoxExtent * 0.55f;
	const FVector AbsDirection(FMath::Abs(OutEvent.Direction.X), FMath::Abs(OutEvent.Direction.Y), FMath::Abs(OutEvent.Direction.Z));
	FVector ProbeAxis = OutEvent.Direction * FVector::DotProduct(ProbeExtent, AbsDirection);
	if (ProbeAxis.SizeSquared() <= 1.0)
	{
		if (ProbeExtent.X >= ProbeExtent.Y && ProbeExtent.X >= ProbeExtent.Z)
		{
			ProbeAxis = FVector(ProbeExtent.X, 0.0, 0.0);
		}
		else if (ProbeExtent.Y >= ProbeExtent.Z)
		{
			ProbeAxis = FVector(0.0, ProbeExtent.Y, 0.0);
		}
		else
		{
			ProbeAxis = FVector(0.0, 0.0, ProbeExtent.Z);
		}
	}
	if (ProbeAxis.SizeSquared() > 1.0)
	{
		const float PositiveDensity = EstimateWarpDensityAtWorldPosition(CurrentBoundsOrigin + ProbeAxis);
		const float NegativeDensity = EstimateWarpDensityAtWorldPosition(CurrentBoundsOrigin - ProbeAxis);
		DensitySum += PositiveDensity + NegativeDensity;
		MaxDensity = FMath::Max(MaxDensity, FMath::Max(PositiveDensity, NegativeDensity));
		DensitySampleCount += 2;
	}
	const float MeanDensity = DensitySum / static_cast<float>(DensitySampleCount);
	const float PathDensity = FMath::Clamp(FMath::Max(MeanDensity, MaxDensity * TimeThiefSmokeParameterDefaults::ActorPushMaxDensityWeight), 0.0f, 1.0f);
	const float ResponseStartSpeed = TimeThiefSmokeParameterDefaults::ActorPushVelocityThreshold * TimeThiefSmokeParameterDefaults::ActorPushResponseStartSpeedScale;
	const float FullResponseSpeed = TimeThiefSmokeParameterDefaults::ActorPushFullResponseSpeed;
	const float ResponseAlpha = TimeThiefSmokeVolume::SmoothStep01((Speed - ResponseStartSpeed) / (FullResponseSpeed - ResponseStartSpeed));
	const float SpeedStrength = FMath::Clamp(Speed / TimeThiefSmokeParameterDefaults::ActorPushFullResponseSpeed, 0.0f, 1.0f);
	const float MotionStrength = FMath::Clamp(ResponseAlpha * SpeedStrength, 0.0f, 1.0f);
	const float OccupancyStrength = PathDensity > TimeThiefSmokeParameterDefaults::ActorPushOccupancyMinDensity
		? FMath::Lerp(TimeThiefSmokeParameterDefaults::ActorPushOccupancyMinStrength, TimeThiefSmokeParameterDefaults::ActorPushOccupancyMaxStrength, PathDensity)
		: 0.0f;
	OutEvent.Strength = FMath::Clamp(FMath::Max(MotionStrength, OccupancyStrength), 0.0f, 1.0f);
	if (OutEvent.Strength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float TravelDistance = FVector::Dist(CurrentBoundsOrigin, PreviousBoundsOrigin);
	const float TravelGate = TimeThiefSmokeVolume::SmoothStep01(TravelDistance / FMath::Max(OutEvent.Radius * TimeThiefSmokeParameterDefaults::ActorPushTravelGateRadiusScale, TimeThiefSmokeParameterDefaults::ActorPushTravelGateMinDistance));
	const float Deposit = PathDensity * TravelGate * FMath::Lerp(TimeThiefSmokeParameterDefaults::ActorWarpDepositMinStrength, 1.0f, MotionStrength);
	WarpAccumulation = FMath::Clamp(WarpAccumulation + Deposit * TimeThiefSmokeParameterDefaults::ActorWarpDensityAccumulationScale, 0.0f, 1.0f);
	OutEvent.WarpBudget = MotionStrength > TimeThiefSmokeParameterDefaults::ActorWarpBudgetMinMotionStrength ? WarpAccumulation : 0.0f;
	WarpAccumulation *= TimeThiefSmokeParameterDefaults::ActorWarpEmissionRemainder;
}

FVector ATimeThiefSmokeVolume::ResolveComponentVelocity(UPrimitiveComponent* PrimitiveComponent, float DeltaTime, FVector& OutPreviousLocation)
{
	if (!PrimitiveComponent)
	{
		OutPreviousLocation = FVector::ZeroVector;
		return FVector::ZeroVector;
	}

	FVector Velocity = PrimitiveComponent->GetComponentVelocity();
	const FVector CurrentLocation = PrimitiveComponent->GetComponentLocation();
	OutPreviousLocation = CurrentLocation;

	if (const FVector* PreviousLocation = PreviousComponentLocations.Find(PrimitiveComponent))
	{
		OutPreviousLocation = *PreviousLocation;
		if (Velocity.IsNearlyZero() && DeltaTime > KINDA_SMALL_NUMBER)
		{
			Velocity = (CurrentLocation - *PreviousLocation) / DeltaTime;
		}
	}

	PreviousComponentLocations.FindOrAdd(PrimitiveComponent) = CurrentLocation;
	return Velocity;
}

float ATimeThiefSmokeVolume::EstimateWarpDensityAtWorldPosition(const FVector& WorldPosition) const
{
	const FTransform SmokeTransform = GetActorTransform();
	const FVector LocalPosition = SmokeTransform.InverseTransformPosition(WorldPosition);
	const FVector NaturalExtent = GetCurrentSmokeBoundsExtent();
	const FVector RenderExtent = GetCurrentSmokeRenderBoundsExtent();
	const FVector NaturalNormalized = LocalPosition / NaturalExtent;
	const FVector RenderNormalized = LocalPosition / RenderExtent;
	const float NaturalDistance = FVector(NaturalNormalized.X, NaturalNormalized.Y, NaturalNormalized.Z / TimeThiefSmokeParameterDefaults::ActorDensityNaturalVerticalScale).Size();
	const float RenderDistance = FVector(RenderNormalized.X, RenderNormalized.Y, RenderNormalized.Z / TimeThiefSmokeParameterDefaults::ActorDensityRenderVerticalScale).Size();
	const float NaturalDensity = 1.0f - TimeThiefSmokeVolume::SmoothStep01((NaturalDistance - TimeThiefSmokeParameterDefaults::ActorDensityNaturalFadeStart) / TimeThiefSmokeParameterDefaults::ActorDensityNaturalFadeWidth);
	const float RenderFade = 1.0f - TimeThiefSmokeVolume::SmoothStep01((RenderDistance - TimeThiefSmokeParameterDefaults::ActorDensityRenderFadeStart) / TimeThiefSmokeParameterDefaults::ActorDensityRenderFadeWidth);
	const float EmissionAlpha = TimeThiefSmokeVolume::SmoothStep01(SmokeAgeSeconds / TimeThiefSmokeParameterDefaults::ActorDensityEmissionWarmupSeconds);
	const float LifetimeAlpha = 1.0f - TimeThiefSmokeVolume::SmoothStep01((SmokeAgeSeconds - TimeThiefSmokeParameterDefaults::SmokeDuration) / TimeThiefSmokeParameterDefaults::SmokeFadeOutDuration);
	const float DensityScale = FMath::Clamp(TimeThiefSmokeParameterDefaults::InitialDensity / TimeThiefSmokeParameterDefaults::ActorDensityScaleDivisor, 0.0f, 1.0f);
	return FMath::Clamp(NaturalDensity * RenderFade * EmissionAlpha * LifetimeAlpha * DensityScale * TimeThiefSmokeParameterDefaults::ActorDensitySampleBoost, 0.0f, 1.0f);
}

ESmokeInteractionShape ATimeThiefSmokeVolume::ResolvePrimitiveShape(UPrimitiveComponent* PrimitiveComponent, FTimeThiefSmokeInteractionEvent& OutEvent) const
{
	if (const USphereComponent* SphereComponent = Cast<USphereComponent>(PrimitiveComponent))
	{
		OutEvent.Radius = SphereComponent->GetScaledSphereRadius();
		OutEvent.Length = 0.0f;
		OutEvent.Extents = FVector(OutEvent.Radius);
		return ESmokeInteractionShape::Sphere;
	}

	if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(PrimitiveComponent))
	{
		OutEvent.Radius = CapsuleComponent->GetScaledCapsuleRadius();
		OutEvent.Length = CapsuleComponent->GetScaledCapsuleHalfHeight() * 2.0f;
		OutEvent.Extents = FVector(OutEvent.Radius, OutEvent.Radius, OutEvent.Length * 0.5f);
		return ESmokeInteractionShape::Capsule;
	}

	if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(PrimitiveComponent))
	{
		OutEvent.Extents = BoxComponent->GetScaledBoxExtent();
		OutEvent.Radius = OutEvent.Extents.GetMax() * TimeThiefSmokeParameterDefaults::ActorPrimitiveRadiusScale;
		OutEvent.Length = OutEvent.Extents.GetMax();
		return ESmokeInteractionShape::Box;
	}

	OutEvent.Extents = PrimitiveComponent->Bounds.BoxExtent;
	OutEvent.Radius = OutEvent.Extents.GetMax() * TimeThiefSmokeParameterDefaults::ActorPrimitiveRadiusScale;
	OutEvent.Length = OutEvent.Extents.GetMax();
	return ESmokeInteractionShape::Box;
}

void ATimeThiefSmokeVolume::MarkObstacleFieldDirty()
{
	bObstacleFieldRebuildPending = true;
}

void ATimeThiefSmokeVolume::RebuildStaticObstacleField()
{
	bObstacleFieldRebuildPending = false;
	ObstaclePrimitives.Reset();
	ObstacleFieldResolution = 0;
	bHasSolidObstacleField = false;

	UWorld* World = GetWorld();
	if (!World || !TimeThiefSmokeParameterDefaults::bUseStaticObstacleMask)
	{
		++ObstacleFieldRevision;
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

	TArray<TWeakObjectPtr<UPrimitiveComponent>> StaticObstacleCandidates;
	TimeThiefSmokeVolume::QuerySmokeObstacleCandidates(
		World,
		this,
		SmokeTransform,
		BoundsExtent,
		ObstacleQueryExtent,
		QueryParams,
		StaticObstacleCandidates);
	StaticObstacleCandidates.Sort([](const TWeakObjectPtr<UPrimitiveComponent>& Left, const TWeakObjectPtr<UPrimitiveComponent>& Right)
	{
		const UPrimitiveComponent* LeftComponent = Left.Get();
		const UPrimitiveComponent* RightComponent = Right.Get();
		const uint32 LeftId = LeftComponent ? LeftComponent->GetUniqueID() : 0u;
		const uint32 RightId = RightComponent ? RightComponent->GetUniqueID() : 0u;
		return LeftId < RightId;
	});

	const uint64 ObstacleCacheKey = TimeThiefSmokeVolume::MakeObstacleMaskCacheKey(
		World,
		SmokeTransform,
		NaturalBoundsExtent,
		BoundsExtent,
		Resolution,
		StaticObstacleCandidates);
	if (const TimeThiefSmokeVolume::FSmokeObstacleFieldCacheEntry* CacheEntry = TimeThiefSmokeVolume::FindObstacleFieldCacheEntry(ObstacleCacheKey))
	{
		if (CacheEntry->ObstacleFieldResolution == Resolution)
		{
			ObstaclePrimitives = CacheEntry->ObstaclePrimitives;
			ObstacleFieldResolution = CacheEntry->ObstacleFieldResolution;
			bHasSolidObstacleField = CacheEntry->bHasSolidObstacleField;
			++ObstacleFieldRevision;
			return;
		}
	}

	ObstaclePrimitives.Reserve(StaticObstacleCandidates.Num());
	for (const TWeakObjectPtr<UPrimitiveComponent>& Candidate : StaticObstacleCandidates)
	{
		if (ObstaclePrimitives.Num() >= TimeThiefSmokeParameterDefaults::MaxObstaclePrimitives)
		{
			break;
		}

		if (TimeThiefSmokeVolume::AddBodySetupObstaclePrimitives(
			Candidate.Get(),
			SmokeTransform,
			ObstaclePrimitives,
			TimeThiefSmokeParameterDefaults::MaxObstaclePrimitives))
		{
			continue;
		}

		FTimeThiefSmokeObstaclePrimitive Primitive;
		if (TimeThiefSmokeVolume::MakeObstaclePrimitive(Candidate.Get(), SmokeTransform, Primitive))
		{
			ObstaclePrimitives.Add(Primitive);
		}
	}
	bHasSolidObstacleField = !ObstaclePrimitives.IsEmpty();

	++ObstacleFieldRevision;
	TimeThiefSmokeVolume::StoreObstacleFieldCacheEntry(
		ObstacleCacheKey,
		ObstaclePrimitives,
		ObstacleFieldResolution,
		bHasSolidObstacleField);
}

void ATimeThiefSmokeVolume::UpdateSmokeBounds()
{
	if (SmokeBoundsComponent)
	{
		SmokeBoundsComponent->SetBoxExtent(GetCurrentSmokeBoundsExtent(), true);
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
	TestFalse(TEXT("Overlap-only component is not a smoke obstacle"), TimeThiefSmokeVolume::IsSmokeProjectileObstacleComponent(BoxComponent, nullptr));

	BoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TestTrue(TEXT("Visibility blocker is a smoke obstacle"), TimeThiefSmokeVolume::IsSmokeProjectileObstacleComponent(BoxComponent, nullptr));

	BoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);
	BoxComponent->ComponentTags.Add(TimeThiefSmokeVolume::BlockProjectileTag);
	TestTrue(TEXT("BlockProjectile tag is a smoke obstacle"), TimeThiefSmokeVolume::IsSmokeProjectileObstacleComponent(BoxComponent, nullptr));

	BoxComponent->ComponentTags.Add(TimeThiefSmokeVolume::IgnoreTag);
	TestFalse(TEXT("Ignore tag excludes projectile blocker"), TimeThiefSmokeVolume::IsSmokeProjectileObstacleComponent(BoxComponent, nullptr));

	return true;
}

#endif
