#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeThiefSmokeDebugVolume.generated.h"

class USceneComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefSmokeDebugVolume : public AActor
{
	GENERATED_BODY()

public:
	ATimeThiefSmokeDebugVolume();

	void InitializeSmokeDebug(float InRadius, float InDuration);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TimeThief|Throwable|Smoke")
	TObjectPtr<USceneComponent> SceneRoot;

	float Radius;

	float Duration;

	int32 DebugSegments;

	FColor DebugColor;
};
