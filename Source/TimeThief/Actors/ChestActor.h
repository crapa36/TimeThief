#pragma once

#include "CoreMinimal.h"
#include "InteractionActorBase.h"
#include "ChestActor.generated.h"

UCLASS()
class TIMETHIEF_API AChestActor : public AInteractionActorBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AChestActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void Interact(const ATimeThiefPlayerCharacter* Player) override;
	
	
};
