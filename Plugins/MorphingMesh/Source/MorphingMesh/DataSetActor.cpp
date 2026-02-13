// Fill out your copyright notice in the Description page of Project Settings.


#include "DataSetActor.h"

#include "MorphingMeshData.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/VolumeTextureBakeFunctions.h"
#include "Settings.h"

// Sets default values
ADataSetActor::ADataSetActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ADataSetActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ADataSetActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ADataSetActor::BakeDataSet() const
{
	if (MorphingMeshData == nullptr)
	{
		return;
	}

	if (!MorphingMeshData->IsValid())
	{
		return;
	}

	if (UDynamicMesh* DynamicMesh = DynamicMeshComponent->GetDynamicMesh())
	{
		FComputeDistanceFieldSettings ComputeDistanceFieldSettings;
		ComputeDistanceFieldSettings.ComputeMode = EDistanceFieldComputeMode::FullGrid;
		ComputeDistanceFieldSettings.NarrowBandUnits = EDistanceFieldUnits::Distance;
		ComputeDistanceFieldSettings.NarrowBandWidth = 0;
		
		FDistanceFieldToTextureSettings DistanceFieldToTextureSettings;
		DistanceFieldToTextureSettings.Offset = 0;
		DistanceFieldToTextureSettings.Scale = 1.0f;
		
		const TArray<TObjectPtr<UStaticMesh>>& BaseMeshes = MorphingMeshData->GetBaseMeshes();

		for (int i = 0; i < 3; ++i)
		{
			ComputeDistanceFieldSettings.VoxelsPerDimensions = FIntVector{NumVoxelsTable[i]};
			
			const TArray<TObjectPtr<UVolumeTexture>>& Textures = MorphingMeshData->GetDensityTextures(i);
			for (int j = 0; j < 3; ++j)
			{
				EGeometryScriptOutcomePins Outcome;
				UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
					BaseMeshes[j],
					DynamicMesh,
					FGeometryScriptCopyMeshFromAssetOptions(),
					FGeometryScriptMeshReadLOD(EGeometryScriptLODType::SourceModel, 0),
					Outcome
				);

				UGeometryScriptLibrary_VolumeTextureBakeFunctions::BakeSignedDistanceToVolumeTexture(
					DynamicMesh,
					Textures[j],
					ComputeDistanceFieldSettings,
					DistanceFieldToTextureSettings
				);
			}
		}
		
		MorphingMeshData->UpdateBox();
	}
}
