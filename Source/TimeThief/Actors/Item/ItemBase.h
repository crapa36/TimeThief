// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../InteractionActorBase.h"
#include "ItemCommons.h"
#include "Interface/PoolObject.h"
#include "ItemBase.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(Blueprintable, BlueprintType)
class TIMETHIEF_API AItemBase : public AInteractionActorBase, public IPoolObject
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<USphereComponent> LookingSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item|VFX", meta=(AllowPrivateAccess=true))
	TObjectPtr<UNiagaraComponent> IdleFXComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|VFX", meta=(AllowPrivateAccess=true))
	TObjectPtr<UNiagaraSystem> IdleFXSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|VFX", meta=(AllowPrivateAccess=true))
	FVector IdleFXOffset = FVector::ZeroVector;

public:
	// Sets default values for this actor's properties
	AItemBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void Interact(const ATimeThiefPlayerCharacter* Player) override;

	virtual void OnBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor, 
		UPrimitiveComponent* OtherComponent, 
		int32 OtherBodyIndex, 
		bool bFromSweep,
		const FHitResult& SweepResult) override;
	
	virtual void OnEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex
	) override;
	
public:
	virtual void Enable() override;
	virtual void Disable() override;
	
	virtual void ApplySpawnRuntimeState(const FNetworkEntityState& EntityState) override;
	
private:
	void TryRequestServer();
	void ActivateIdleFX();
	void DeactivateIdleFX();
	
public:
	UFUNCTION(BlueprintCallable)
	void SetItemStack(EItemID NewItemID, int NewQuantity);

	EItemID GetItemID() const { return ItemID; }
	int GetQuantity() const { return Quantity; }
	
protected:
	UPROPERTY(EditAnywhere, meta=(AllowPrivateAccess=true), Category="Item")
	EItemID ItemID;

	UPROPERTY(EditAnywhere, meta=(AllowPrivateAccess=true), Category="Item")
	int Quantity = 1;
};
