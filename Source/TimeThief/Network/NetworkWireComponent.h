#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/Wire/TimeThiefWireTypes.h"
#include "NetworkWireComponent.generated.h"

class UTimeThiefWireComponent;
class UNetworkGameInstanceSubsystem;

UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UNetworkWireComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNetworkWireComponent();

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleLocalWireAttached(const FVector& AnchorPoint);

	UFUNCTION()
	void HandleLocalWireStateChanged(EWireState OldState, EWireState NewState);

	UFUNCTION()
	void HandleLocalWireLaunched(const FVector& StartPosition, const FVector& Direction);

	bool IsLocalControlledOwner() const;
	UNetworkGameInstanceSubsystem* GetNetworkGameInstanceSubsystem();

private:
	UPROPERTY(Transient)
	TObjectPtr<UTimeThiefWireComponent> WireComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNetworkGameInstanceSubsystem> NGIS = nullptr;
};


