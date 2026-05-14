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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Smoke", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float Radius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Smoke", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float Duration = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Smoke", meta = (ClampMin = "4", UIMin = "4"))
	int32 DebugSegments = 32;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Throwable|Smoke")
	FColor DebugColor = FColor::Silver;
};
