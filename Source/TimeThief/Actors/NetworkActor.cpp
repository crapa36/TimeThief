
#include "NetworkActor.h"
#include "Network/NetworkEntityComponent.h"


// Sets default values
ANetworkActor::ANetworkActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ANetworkActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ANetworkActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

UNetworkEntityComponent* ANetworkActor::GetNetworkEntityComponent() const
{
	return NetworkEntityComponent;
}

uint32 ANetworkActor::GetEntityId() const
{
	return NetworkEntityComponent ? NetworkEntityComponent->GetEntityId() : 0;
}

