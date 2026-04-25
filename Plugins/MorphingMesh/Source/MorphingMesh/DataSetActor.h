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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	TObjectPtr<UMorphingMeshData> MorphingMeshData;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	int Resolution = 128;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	void CreateBoneIndexTexture() const;
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(CallInEditor, Category = "Default")
	void BakeDataSet() const;
	
	UPROPERTY(EditAnywhere, Category = "Default")
	TArray<FString> ExcludedBoneKeywords;
	
	// 스태틱 메쉬에만 존재하는 정점들에 할당할 본 이름
	UPROPERTY(EditAnywhere, Category = "Default")
	FName ExtraBoneName = NAME_None;

	// 스켈레탈 메쉬 정점과의 거리가 이 값보다 멀면 '스태틱 메쉬에만 있는 정점'으로 간주합니다.
	UPROPERTY(EditAnywhere, Category = "Default")
	float ExtraVertexDistanceThreshold = 2.0f;
};
