#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TimeThiefWireTargeting.generated.h"

class ACharacter;

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class TIMETHIEF_API UTimeThiefWireTargeting : public UObject
{
	GENERATED_BODY()

public:
	UTimeThiefWireTargeting();

	virtual void Initialize(ACharacter* InCharacter);

	virtual bool FindBestAnchorTarget(FVector& OutTargetLocation, const FVector& StartLocation, const FVector& AimDirection, float MaxLength);
	virtual bool CheckAnchorCollision(const FVector& Start, const FVector& End, FHitResult& OutHit, AActor* IgnoredActor);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float AutoAimRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float MinTargetDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	bool bAllowFloorAttachment = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float LedgeCheckHeight = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	float AimAccuracyWeight = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Targeting")
	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionObjectTypes;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CachedCharacter;
};
