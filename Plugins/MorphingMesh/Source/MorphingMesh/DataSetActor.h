// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/GeometryFramework/Public/DynamicMeshActor.h"
#include "DataSetActor.generated.h"

class UMorphingMeshData;

UCLASS()
class MORPHINGMESH_API ADataSetActor : public ADynamicMeshActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADataSetActor();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="DataSet")
	TObjectPtr<UMorphingMeshData> MorphingMeshData;
	
	UPROPERTY(EditAnywhere, Category="Settings")
	int Resolution = 128;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	void CreateBoneIndexTexture() const;
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(CallInEditor)
	void BakeDataSet() const;
	
	UPROPERTY(EditAnywhere, Category="DataSet | Settings")
	TArray<FString> ExcludedBoneKeywords;
};
