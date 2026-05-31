#pragma once

#include "CoreMinimal.h"
#include "TimeThiefSmokeParameterDefaults.h"

enum class ETimeThiefSmokeRendererInteractionType : uint8
{
	BulletWake = 0,
	ExplosionShock = 1,
	ActorPush = 2,
	PlumeSource = 3
};

enum class ETimeThiefSmokeRendererInteractionShape : uint8
{
	Sphere = 0,
	Capsule = 1,
	Box = 2,
	LineWake = 3
};

enum class ETimeThiefSmokeSimulationBackend : uint8
{
	DenseLegacy = 0,
	SparseMac = 1
};

enum class ETimeThiefSmokeObstaclePrimitiveShape : uint8
{
	Sphere = 0,
	Capsule = 1,
	Box = 2,
	Aabb = 3
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeObstaclePrimitive
{
	FVector4f CenterRadius = FVector4f::Zero();
	FVector4f AxisHalfLength = FVector4f::Zero();
	FVector4f ExtentsShape = FVector4f::Zero();
	FVector4f Rotation = FVector4f::Zero();
	FVector4f Velocity = FVector4f::Zero();
	FVector4f AngularVelocity = FVector4f::Zero();
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererEvent
{
	int32 SmokeId = INDEX_NONE;
	ETimeThiefSmokeRendererInteractionType Type = ETimeThiefSmokeRendererInteractionType::BulletWake;
	ETimeThiefSmokeRendererInteractionShape Shape = ETimeThiefSmokeRendererInteractionShape::Sphere;
	FVector3f Position = FVector3f::ZeroVector;
	FVector3f PreviousPosition = FVector3f::ZeroVector;
	FVector3f Direction = FVector3f::ForwardVector;
	FQuat4f Rotation = FQuat4f::Identity;
	FVector3f Extents = FVector3f::ZeroVector;
	float Radius = 0.0f;
	float Length = 0.0f;
	float Strength = 1.0f;
	float Speed = 0.0f;
	float NormalizedAge = 0.0f;
	int32 Seed = 0;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererVolume
{
	FTimeThiefSmokeRendererVolume();

	int32 SmokeId = INDEX_NONE;
	FTransform3f LocalToWorld = FTransform3f::Identity;
	FVector3f BoundsExtent;
	FVector3f SimulationBoundsExtent;
	FVector3f NaturalBoundsExtent;
	FVector3f RenderBoundsExtent;
	float AgeSeconds = 0.0f;
	float DurationSeconds;
	int32 ObstacleFieldResolution = 0;
	uint32 ObstacleFieldRevision = 0;
	TArray<FTimeThiefSmokeObstaclePrimitive> ObstaclePrimitives;
	bool bHasSolidObstacleField = false;
};

struct TIMETHIEFSMOKERENDERER_API FTimeThiefSmokeRendererFrame
{
	uint64 SceneKey = 0;
	float DeltaSeconds = 0.0f;
	TArray<FTimeThiefSmokeRendererVolume> Volumes;
	TArray<FTimeThiefSmokeRendererEvent> Events;
};
