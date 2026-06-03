
#include "NetworkActor.h"
#include "Network/NetworkEntityComponent.h"


// Sets default values
ANetworkActor::ANetworkActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	NetworkEntityComponent = CreateDefaultSubobject<UNetworkEntityComponent>(TEXT("NetworkEntityComponent"));
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

