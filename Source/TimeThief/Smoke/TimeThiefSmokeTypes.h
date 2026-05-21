#pragma once

#include "CoreMinimal.h"
#include "TimeThiefSmokeTypes.generated.h"

class ATimeThiefSmokeVolume;

UENUM(BlueprintType)
enum class ESmokeInteractionType : uint8
{
	BulletWake,
	ExplosionShock,
	ActorPush
};

UENUM(BlueprintType)
enum class ESmokeInteractionShape : uint8
{
	Sphere,
	Capsule,
	Box,
	LineWake
};

USTRUCT(BlueprintType)
struct FTimeThiefSmokeInteractionEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	int32 SmokeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	ESmokeInteractionType Type = ESmokeInteractionType::BulletWake;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	ESmokeInteractionShape Shape = ESmokeInteractionShape::Sphere;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector PreviousPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	FVector Extents = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float Radius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float Length = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float Strength = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float WarpBudget = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	float NormalizedAge = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Smoke")
	int32 Seed = 0;
};
