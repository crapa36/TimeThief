// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractionActorBase.h"
#include "StoreActor.generated.h"

class USphereComponent;

UCLASS()
class TIMETHIEF_API AStoreActor : public AInteractionActorBase
{
	GENERATED_BODY()
public:
	// Sets default values for this actor's properties
	AStoreActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void OnEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex
	) override;
	
	virtual void Interact(const ATimeThiefPlayerCharacter* Player) override;
};
