#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "NetworkCombatSyncComponent.generated.h"


struct FRemoteAttackNotify;
struct FCombatAttackRequest;
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRemoteAttackNotify, const FRemoteAttackNotify&);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UNetworkCombatSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UNetworkCombatSyncComponent();
	
public:
	FOnRemoteAttackNotify OnRemoteAttackNotify;
	
public:
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void HandleLocalAttackRequest(const FCombatAttackRequest& AttackRequest);
	void BroadcastRemoteAttackNotify(const FRemoteAttackNotify& AttackNotify) const;
	
private:
	class UTimeThiefPawnCombatComponent* TTCombatComponent = nullptr;

};
