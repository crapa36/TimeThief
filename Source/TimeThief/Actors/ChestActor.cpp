


#include "ChestActor.h"
#include "Network/NetworkGameInstanceSubsystem.h"


// Sets default values
AChestActor::AChestActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AChestActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AChestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AChestActor::Interact(const ATimeThiefPlayerCharacter* Player)
{
	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		uint32 ChestEntityId = GetEntityId();
		NGIS->SendChestInteract(ChestEntityId);
	}
}

