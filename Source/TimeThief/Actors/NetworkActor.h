#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Network/NetworkEntityInterface.h"
#include "NetworkActor.generated.h"

UCLASS()
class TIMETHIEF_API ANetworkActor : public AActor
	, public INetworkEntityInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANetworkActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
public:
	virtual UNetworkEntityComponent* GetNetworkEntityComponent() const override;
	uint32 GetEntityId() const;
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNetworkEntityComponent> NetworkEntityComponent = nullptr;
	
};
