#pragma once

#include "CoreMinimal.h"
#include "Character/TimeThiefNetworkCharacterBase.h"

#include "NTPlayer.generated.h"

struct FNetworkEntityState;

UCLASS()
class TIMETHIEF_API ANTPlayer : public ATimeThiefNetworkCharacterBase
// Network Test Player
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ANTPlayer();
	virtual ~ANTPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
};
