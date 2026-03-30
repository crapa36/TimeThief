// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MorphingMeshComponent.generated.h"


class UMorphingMeshData;
class ULiquidMeshComponent;

UENUM(BlueprintType)
enum class EMorphTargetType : uint8
{
	A = 0 UMETA(DisplayName = "Morph Target A"),
	B UMETA(DisplayName = "Morph Target B"),
	C UMETA(DisplayName = "Morph Target C"),
	None
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MORPHINGMESH_API UMorphingMeshComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<UMorphingMeshData> MorphingMeshData;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BaseMeshComponent;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<ULiquidMeshComponent> LiquidMeshComponent;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInterface> LiquidMaterial;
public:
	// Sets default values for this component's properties
	UMorphingMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void OnRegister() override;
public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintCallable)
	void SetType(EMorphTargetType NewType);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morphing | Settings", meta=(ClampMin="0.1", ClampMax="1.0", UIMin="0.1", UIMax="1.0"))
	float MaxMorphingTime{0.5f};
	
	float MorphingTime{MaxMorphingTime};
	float ElapsedTime{0.0f};
	
	FVector3f PrevAlpha{0.0f, 0.0f, 0.0f};
	FVector3f CurrAlpha{0.0f, 0.0f, 0.0f};
	FVector3f DestAlpha{0.0f, 0.0f, 0.0f};
	
	EMorphTargetType MeshType{EMorphTargetType::None};
};
