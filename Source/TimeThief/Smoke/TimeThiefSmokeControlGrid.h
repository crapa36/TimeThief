#pragma once

#include "CoreMinimal.h"
#include "Smoke/TimeThiefSmokeTypes.h"

class UWorld;

struct FTimeThiefSmokeControlGrid
{
	void Initialize(
		UWorld* World,
		const FTransform& InGridTransform,
		const FVector& InBoundsExtent,
		int32 InResolution,
		float InInitialDensity,
		const AActor* InOwnerToIgnore);

	void Reset();
	void Tick(float DeltaTime);
	void ApplyInteractionEvent(const FTimeThiefSmokeInteractionEvent& Event);

	bool IsInitialized() const { return Resolution > 0 && Density.Num() > 0; }
	float GetDensityAtIndex(int32 Index) const { return Density.IsValidIndex(Index) ? Density[Index] : 0.0f; }

private:
	int32 ToIndex(int32 X, int32 Y, int32 Z) const;
	bool IsValidCell(int32 X, int32 Y, int32 Z) const;
	FVector GetCellWorldPosition(int32 X, int32 Y, int32 Z) const;
	float ComputeShapeWeight(const FTimeThiefSmokeInteractionEvent& Event, const FVector& WorldPosition) const;
	FVector ComputeCurlDirection(const FVector& WorldPosition, int32 Seed) const;
	void SampleStaticObstacles(UWorld* World, const AActor* OwnerToIgnore);
	void ClampObstacleCell(int32 Index);

	FTransform GridTransform = FTransform::Identity;
	FVector BoundsExtent = FVector::ZeroVector;
	int32 Resolution = 0;
	float InitialDensity = 1.0f;

	TArray<float> Density;
	TArray<float> ScratchDensity;
	TArray<FVector> Velocity;
	TArray<uint8> ObstacleMask;
};
