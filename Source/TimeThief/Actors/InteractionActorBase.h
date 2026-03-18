// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractionActorBase.generated.h"

class UWidgetComponent;
class ATimeThiefPlayerCharacter;
class USphereComponent;

UCLASS()
class TIMETHIEF_API AInteractionActorBase : public AActor
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UStaticMeshComponent> MeshComponent;
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<USphereComponent> InteractionSphere;
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UWidgetComponent> InteractionWidgetComponent;
	
public:
	// Sets default values for this actor's properties
	AInteractionActorBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	void SetVisibilityInteractionUI(bool bShow);
	
	UFUNCTION()
	virtual void OnBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	){}

	UFUNCTION()
	virtual void OnEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex
	){}
	
	virtual void Interact(const ATimeThiefPlayerCharacter* Player){}
	
protected:
	UPROPERTY(EditDefaultsOnly)
	int Priority = 1000;
};
