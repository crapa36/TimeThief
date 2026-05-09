#include "Smoke/TimeThiefSmokeControlGrid.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"

namespace TimeThiefSmokeGrid
{
	constexpr float VelocityDampingPerSecond = 4.0f;
	constexpr float DensityRecoveryPerSecond = 2.35f;
	constexpr float DensityDiffusionPerSecond = 0.12f;
	constexpr float MinBulletDensityScale = 0.45f;

	float SmoothFalloff(float NormalizedDistance)
	{
		const float T = FMath::Clamp(NormalizedDistance, 0.0f, 1.0f);
		return 1.0f - (T * T * (3.0f - 2.0f * T));
	}
}

void FTimeThiefSmokeControlGrid::Initialize(
	UWorld* World,
	const FTransform& InGridTransform,
	const FVector& InBoundsExtent,
	int32 InResolution,
	float InInitialDensity,
	const AActor* InOwnerToIgnore)
{
	Reset();

	Resolution = FMath::Clamp(InResolution, 8, 64);
	GridTransform = InGridTransform;
	BoundsExtent = InBoundsExtent.ComponentMax(FVector(1.0f));
	InitialDensity = FMath::Max(0.0f, InInitialDensity);

	const int32 CellCount = Resolution * Resolution * Resolution;
	Density.Init(InitialDensity, CellCount);
	ScratchDensity.Init(InitialDensity, CellCount);
	Velocity.Init(FVector::ZeroVector, CellCount);
	ObstacleMask.Init(0, CellCount);

	if (World)
	{
		SampleStaticObstacles(World, InOwnerToIgnore);
	}
}

void FTimeThiefSmokeControlGrid::Reset()
{
	GridTransform = FTransform::Identity;
	BoundsExtent = FVector::ZeroVector;
	Resolution = 0;
	InitialDensity = 1.0f;
	Density.Reset();
	ScratchDensity.Reset();
	Velocity.Reset();
	ObstacleMask.Reset();
}

void FTimeThiefSmokeControlGrid::Tick(float DeltaTime)
{
	if (!IsInitialized() || DeltaTime <= 0.0f)
	{
		return;
	}

	const float VelocityDamping = FMath::Exp(-TimeThiefSmokeGrid::VelocityDampingPerSecond * DeltaTime);
	const float RecoveryAlpha = 1.0f - FMath::Exp(-TimeThiefSmokeGrid::DensityRecoveryPerSecond * DeltaTime);
	const float DiffusionAlpha = FMath::Clamp(TimeThiefSmokeGrid::DensityDiffusionPerSecond * DeltaTime, 0.0f, 0.25f);

	for (int32 Z = 0; Z < Resolution; ++Z)
	{
		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			for (int32 X = 0; X < Resolution; ++X)
			{
				const int32 Index = ToIndex(X, Y, Z);
				if (ObstacleMask[Index] != 0)
				{
					ScratchDensity[Index] = 0.0f;
					Velocity[Index] = FVector::ZeroVector;
					continue;
				}

				float NeighborSum = 0.0f;
				int32 NeighborCount = 0;
				const FIntVector Offsets[] =
				{
					FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
					FIntVector(0, 1, 0), FIntVector(0, -1, 0),
					FIntVector(0, 0, 1), FIntVector(0, 0, -1)
				};

				for (const FIntVector& Offset : Offsets)
				{
					const int32 NX = X + Offset.X;
					const int32 NY = Y + Offset.Y;
					const int32 NZ = Z + Offset.Z;
					if (IsValidCell(NX, NY, NZ))
					{
						const int32 NeighborIndex = ToIndex(NX, NY, NZ);
						if (ObstacleMask[NeighborIndex] == 0)
						{
							NeighborSum += Density[NeighborIndex];
							++NeighborCount;
						}
					}
				}

				const float NeighborAverage = NeighborCount > 0 ? NeighborSum / static_cast<float>(NeighborCount) : Density[Index];
				float NewDensity = FMath::Lerp(Density[Index], NeighborAverage, DiffusionAlpha);
				NewDensity = FMath::Lerp(NewDensity, InitialDensity, RecoveryAlpha);
				ScratchDensity[Index] = FMath::Clamp(NewDensity, 0.0f, InitialDensity);
				Velocity[Index] *= VelocityDamping;
			}
		}
	}

	Swap(Density, ScratchDensity);
}

void FTimeThiefSmokeControlGrid::ApplyInteractionEvent(const FTimeThiefSmokeInteractionEvent& Event)
{
	if (!IsInitialized())
	{
		return;
	}

	const FVector Direction = Event.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const float SafeStrength = FMath::Max(0.0f, Event.Strength);
	const float Radius = FMath::Max(1.0f, Event.Radius);
	const float OutwardStrength = Event.Extents.X > 0.0f ? Event.Extents.X : 900.0f;
	const float ExplosionClearStrength = FMath::Clamp(Event.Extents.Y, 0.0f, 1.0f);

	for (int32 Z = 0; Z < Resolution; ++Z)
	{
		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			for (int32 X = 0; X < Resolution; ++X)
			{
				const int32 Index = ToIndex(X, Y, Z);
				if (ObstacleMask[Index] != 0)
				{
					ClampObstacleCell(Index);
					continue;
				}

				const FVector WorldPosition = GetCellWorldPosition(X, Y, Z);
				const float Weight = ComputeShapeWeight(Event, WorldPosition);
				if (Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const FVector Curl = ComputeCurlDirection(WorldPosition, Event.Seed);
				switch (Event.Type)
				{
				case ESmokeInteractionType::BulletWake:
				{
					const float DensityScale = FMath::Lerp(1.0f, TimeThiefSmokeGrid::MinBulletDensityScale, Weight * SafeStrength);
					Density[Index] = FMath::Clamp(Density[Index] * DensityScale, InitialDensity * 0.2f, InitialDensity);
					Velocity[Index] += ((Direction * 420.0f) + (Curl * 115.0f)) * Weight * SafeStrength;
					break;
				}
				case ESmokeInteractionType::ExplosionShock:
				{
					const FVector RadialDirection = (WorldPosition - Event.Position).GetSafeNormal(UE_SMALL_NUMBER, Curl);
					const float Shell = FMath::Clamp(FMath::Abs((FVector::Distance(WorldPosition, Event.Position) / Radius) - 0.78f) / 0.22f, 0.0f, 1.0f);
					const float ShellPreserve = 1.0f - Shell;
					const float DensityReduction = ExplosionClearStrength * 0.35f * Weight * SafeStrength;
					Density[Index] = FMath::Clamp(Density[Index] * (1.0f - DensityReduction) + (InitialDensity * 0.08f * ShellPreserve), 0.0f, InitialDensity * 1.15f);
					Velocity[Index] += ((RadialDirection * OutwardStrength) + (FVector::UpVector * OutwardStrength * 0.14f) + (Curl * OutwardStrength * 0.18f)) * Weight * SafeStrength;
					break;
				}
				case ESmokeInteractionType::ActorPush:
				{
					Density[Index] = FMath::Clamp(Density[Index] * (1.0f - 0.28f * Weight * SafeStrength), 0.0f, InitialDensity);
					Velocity[Index] += ((Direction * 360.0f) + (Curl * 80.0f)) * Weight * SafeStrength;
					break;
				}
				default:
					break;
				}
			}
		}
	}
}

int32 FTimeThiefSmokeControlGrid::ToIndex(int32 X, int32 Y, int32 Z) const
{
	return X + (Y * Resolution) + (Z * Resolution * Resolution);
}

bool FTimeThiefSmokeControlGrid::IsValidCell(int32 X, int32 Y, int32 Z) const
{
	return X >= 0 && X < Resolution && Y >= 0 && Y < Resolution && Z >= 0 && Z < Resolution;
}

FVector FTimeThiefSmokeControlGrid::GetCellWorldPosition(int32 X, int32 Y, int32 Z) const
{
	const FVector Alpha(
		(static_cast<float>(X) + 0.5f) / static_cast<float>(Resolution),
		(static_cast<float>(Y) + 0.5f) / static_cast<float>(Resolution),
		(static_cast<float>(Z) + 0.5f) / static_cast<float>(Resolution));

	const FVector LocalPosition = ((Alpha * 2.0f) - FVector::OneVector) * BoundsExtent;
	return GridTransform.TransformPosition(LocalPosition);
}

float FTimeThiefSmokeControlGrid::ComputeShapeWeight(const FTimeThiefSmokeInteractionEvent& Event, const FVector& WorldPosition) const
{
	switch (Event.Shape)
	{
	case ESmokeInteractionShape::LineWake:
	{
		const FVector Direction = Event.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector HalfSegment = Direction * FMath::Max(1.0f, Event.Length * 0.5f);
		const FVector SegmentStart = Event.Position - HalfSegment;
		const FVector SegmentEnd = Event.Position + HalfSegment;
		const FVector Closest = FMath::ClosestPointOnSegment(WorldPosition, SegmentStart, SegmentEnd);
		return TimeThiefSmokeGrid::SmoothFalloff(FVector::Distance(WorldPosition, Closest) / FMath::Max(1.0f, Event.Radius));
	}
	case ESmokeInteractionShape::Capsule:
	{
		const FVector Direction = Event.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector HalfSegment = Direction * FMath::Max(0.0f, Event.Length * 0.5f);
		const FVector SegmentStart = Event.Position - HalfSegment;
		const FVector SegmentEnd = Event.Position + HalfSegment;
		const FVector Closest = FMath::ClosestPointOnSegment(WorldPosition, SegmentStart, SegmentEnd);
		return TimeThiefSmokeGrid::SmoothFalloff(FVector::Distance(WorldPosition, Closest) / FMath::Max(1.0f, Event.Radius));
	}
	case ESmokeInteractionShape::Box:
	{
		const FVector LocalPosition = Event.Rotation.Inverse().RotateVector(WorldPosition - Event.Position);
		const FVector Delta = (LocalPosition.GetAbs() - Event.Extents).ComponentMax(FVector::ZeroVector);
		return TimeThiefSmokeGrid::SmoothFalloff(Delta.Size() / FMath::Max(1.0f, Event.Radius));
	}
	case ESmokeInteractionShape::Sphere:
	default:
		return TimeThiefSmokeGrid::SmoothFalloff(FVector::Distance(WorldPosition, Event.Position) / FMath::Max(1.0f, Event.Radius));
	}
}

FVector FTimeThiefSmokeControlGrid::ComputeCurlDirection(const FVector& WorldPosition, int32 Seed) const
{
	const float X = FMath::Sin((WorldPosition.X * 0.017f) + static_cast<float>(Seed) * 0.37f);
	const float Y = FMath::Cos((WorldPosition.Y * 0.013f) + static_cast<float>(Seed) * 0.19f);
	const float Z = FMath::Sin((WorldPosition.Z * 0.011f) + static_cast<float>(Seed) * 0.23f);
	return FVector(Y - Z, Z - X, X - Y).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
}

void FTimeThiefSmokeControlGrid::SampleStaticObstacles(UWorld* World, const AActor* OwnerToIgnore)
{
	if (!World || !IsInitialized())
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TimeThiefSmokeObstacleSample), false);
	if (OwnerToIgnore)
	{
		QueryParams.AddIgnoredActor(OwnerToIgnore);
	}

	const float CellRadius = BoundsExtent.GetMax() / static_cast<float>(Resolution) * 0.65f;
	const FCollisionShape CellShape = FCollisionShape::MakeSphere(FMath::Max(2.0f, CellRadius));

	for (int32 Z = 0; Z < Resolution; ++Z)
	{
		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			for (int32 X = 0; X < Resolution; ++X)
			{
				const int32 Index = ToIndex(X, Y, Z);
				const FVector WorldPosition = GetCellWorldPosition(X, Y, Z);
				const bool bBlocked = World->OverlapBlockingTestByChannel(
					WorldPosition,
					FQuat::Identity,
					ECC_WorldStatic,
					CellShape,
					QueryParams);

				if (bBlocked)
				{
					ObstacleMask[Index] = 1;
					ClampObstacleCell(Index);
				}
			}
		}
	}
}

void FTimeThiefSmokeControlGrid::ClampObstacleCell(int32 Index)
{
	if (Density.IsValidIndex(Index))
	{
		Density[Index] = 0.0f;
	}
	if (ScratchDensity.IsValidIndex(Index))
	{
		ScratchDensity[Index] = 0.0f;
	}
	if (Velocity.IsValidIndex(Index))
	{
		Velocity[Index] = FVector::ZeroVector;
	}
}
