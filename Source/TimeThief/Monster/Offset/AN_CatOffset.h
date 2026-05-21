

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_CatOffset.generated.h"

/**
 * 
 */
UCLASS()
class TIMETHIEF_API UAN_CatOffset : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SocketName = TEXT("FX_Cannon_Socket");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LocationOffset = FVector::ZeroVector;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation
	) override;
	
};
