// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractionActorBase.h"
#include "ItemCommons.h"
#include "ItemBase.generated.h"

UCLASS(Blueprintable, BlueprintType)
class TIMETHIEF_API AItemBase : public AInteractionActorBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AItemBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

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
	
	virtual void Interact(const ATimeThiefPlayerCharacter* Player) override;
	
protected:
	UPROPERTY(EditAnywhere, meta=(AllowPrivateAccess=true), Category="Item Name")
	EItemName ItemName;
};
