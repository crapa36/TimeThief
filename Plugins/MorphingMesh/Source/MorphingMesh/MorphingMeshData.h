// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/VolumeTexture.h"
#include "MorphingMeshData.generated.h"

/**
 * 
 */
USTRUCT()
struct FDensitySet
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, EditFixedSize, meta=(EditFixedOrder))
	TArray<TObjectPtr<UVolumeTexture>> DensityTexture;
	
	FDensitySet();
	
	bool IsValid() const;
};

UCLASS()
class MORPHINGMESH_API UMorphingMeshData : public UDataAsset
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, EditFixedSize, meta=(EditFixedOrder))
	TArray<TObjectPtr<UStaticMesh>> BaseMeshes;
	
	UPROPERTY(EditDefaultsOnly, EditFixedSize, meta=(EditFixedOrder))
	TArray<FDensitySet> DensityTextures;
	
	UPROPERTY(EditDefaultsOnly, EditFixedSize, meta=(EditFixedOrder))
	TArray<FBox> Bounds;
	
public:
	UMorphingMeshData();
	
	const TArray<TObjectPtr<UStaticMesh>>& GetBaseMeshes() const { return BaseMeshes; }
	const TArray<TObjectPtr<UVolumeTexture>>& GetDensityTextures(int Index) const { return DensityTextures[Index].DensityTexture; }
	const TArray<FBox>& GetBounds() const { return Bounds; }
	
	void UpdateBox();
	bool IsValid() const;
};
