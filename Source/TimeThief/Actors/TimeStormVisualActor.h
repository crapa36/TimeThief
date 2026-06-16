// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeStormVisualActor.generated.h"

UCLASS()
class TIMETHIEF_API ATimeStormVisualActor : public AActor
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ZoneMesh;
	
	UPROPERTY(EditAnywhere, Category = "PostProcess")
	TObjectPtr<UMaterialInterface> BasePostProcessMaterial;
	
public:
	// Sets default values for this actor's properties
	ATimeStormVisualActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicPostProcessMaterial;
	
	UPROPERTY()
	TSoftObjectPtr<APostProcessVolume> TargetPPVolume;
};
