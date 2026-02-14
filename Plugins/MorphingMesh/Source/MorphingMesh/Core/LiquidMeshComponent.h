// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "LiquidMeshComponent.generated.h"

class UMorphingMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MORPHINGMESH_API ULiquidMeshComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UPROPERTY()
	const UMorphingMeshComponent* ParentComponent = nullptr;

	// BeginPlay 이후 플레이어 여부 캐시
	bool bIsPlayerControlled = false;
	bool bRenderingEnable = false;
public:
	ULiquidMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	bool IsPlayerControlled() const { return bIsPlayerControlled; }

	virtual bool ShouldRecreateProxyOnUpdateTransform() const override { return false; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void OnRegister() override;
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void DestroyRenderState_Concurrent() override;
	
	FBox GetBound() const;
	FVector3f GetAlpha() const;
	TArray<TObjectPtr<UVolumeTexture>> GetDensityTextures() const;
};
