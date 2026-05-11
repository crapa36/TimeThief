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

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMorphTargetTypeChangedSignature, EMorphTargetType);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MORPHINGMESH_API UMorphingMeshComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	USkeletalMeshComponent* BaseSkeletalMeshComponent{nullptr};
	
	UPROPERTY(EditAnywhere, Category = "Morphing | Settings")
	TObjectPtr<UMorphingMeshData> MorphingMeshData;

	UPROPERTY(VisibleAnywhere, Category = "Morphing | Settings")
	TObjectPtr<UStaticMeshComponent> BaseMeshComponent;
	
	UPROPERTY(EditAnywhere, Category = "Morphing | Settings")
	TObjectPtr<ULiquidMeshComponent> LiquidMeshComponent;
	
	UPROPERTY(EditAnywhere, Category = "Morphing | Settings")
	TObjectPtr<UMaterialInterface> LiquidMaterial;
	
	FOnMorphTargetTypeChangedSignature OnMorphTargetTypeChangedSignature;
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
	
	void SetSkeletalMeshComponent(USkeletalMeshComponent* NewSkeletalMeshComponent);
	
	int GetActiveSkeletalIndex() const;
	
	UFUNCTION(BlueprintCallable)
	void SetType(EMorphTargetType NewType);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morphing | Settings")
	float MaxMorphingTime{0.5f};
	
	float MorphingTime{MaxMorphingTime};
	float ElapsedTime{0.0f};
	
	FVector PrevAlpha{0.0f, 1.0f, 0.0f};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morphing | Settings")
	FVector CurrAlpha{0.0f, 1.0f, 0.0f};
	FVector DestAlpha{0.0f, 1.0f, 0.0f};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morphing | Settings")
	EMorphTargetType MeshType{EMorphTargetType::A};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Morphing | Settings")
	bool bDebug = false;
	
	bool bIsSkeletalMesh{false};
	bool bIsValid{false};
	bool bIsDifferentSkeletal{false};
	
	int PrevIndex{-1};
	
private:
	void Check();
};
