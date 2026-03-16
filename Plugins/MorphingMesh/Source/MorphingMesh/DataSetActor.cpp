// Fill out your copyright notice in the Description page of Project Settings.


#include "DataSetActor.h"

#include "MorphingMeshData.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/VolumeTextureBakeFunctions.h"
#include "GeometryScript/MeshSpatialFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "Math/Float16Color.h"
#include "Engine/VolumeTexture.h"
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
		const TArray<TObjectPtr<UVolumeTexture>>&  UVVolumeTextures = MorphingMeshData->GetUVVolumeTextures();

		for (int i = 0; i < 3; ++i)
		{
			auto TextureSets = MorphingMeshData->GetDensityTextureSets();

			EGeometryScriptOutcomePins Outcome;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				BaseMeshes[i],
				DynamicMesh,
				FGeometryScriptCopyMeshFromAssetOptions(),
				FGeometryScriptMeshReadLOD(EGeometryScriptLODType::SourceModel, 0),
				Outcome
			);

			for (int j = 0; j < 3; ++j)
			{
				ComputeDistanceFieldSettings.VoxelsPerDimensions = FIntVector{NumVoxelsTable[j]};

				UGeometryScriptLibrary_VolumeTextureBakeFunctions::BakeSignedDistanceToVolumeTexture(
					DynamicMesh,
					TextureSets[j].DensityTexture[i],
					ComputeDistanceFieldSettings,
					DistanceFieldToTextureSettings
				);
			}

			// 1. 메시의 바운딩 박스 구하기
			FBox Bounds = BaseMeshes[i]->GetBoundingBox();
			FVector Min = Bounds.Min;
			FVector Max = Bounds.GetSize();

			// 2. BVH(공간 가속 구조) 빌드 - 검색 속도를 미친듯이 올려줍니다.
			FGeometryScriptDynamicMeshBVH BVH;
			UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(DynamicMesh, BVH);

			// 3. 복셀 데이터를 담을 1차원 배열 생성 (크기: Res * Res * Res)
			int32 TotalVoxels = Resolution * Resolution * Resolution;
			TArray<FFloat16Color> VoxelData;
			VoxelData.Init(FFloat16Color(FLinearColor::Black), TotalVoxels);

			// 4. 3중 루프 (Z -> Y -> X 순서)
			for (int32 Z = 0; Z < Resolution; ++Z)
			{
				for (int32 Y = 0; Y < Resolution; ++Y)
				{
					for (int32 X = 0; X < Resolution; ++X)
					{
						// 현재 복셀의 비율 (0.0 ~ 1.0)
						float AlphaX = (float)X / (float)(Resolution - 1);
						float AlphaY = (float)Y / (float)(Resolution - 1);
						float AlphaZ = (float)Z / (float)(Resolution - 1);
			
						// 실제 3D 공간상의 Query Point 계산 (Lerp)
						FVector QueryPoint = Min + FVector(AlphaX, AlphaY, AlphaZ) * Max;
			
						// 가장 가까운 지점 찾기
						FGeometryScriptSpatialQueryOptions NearestOptions;
						FGeometryScriptTrianglePoint NearestResult;
						EGeometryScriptSearchOutcomePins NearestOutcome;
			
						UGeometryScriptLibrary_MeshSpatial::FindNearestPointOnMesh(
							DynamicMesh, BVH, QueryPoint, NearestOptions, NearestResult, NearestOutcome);
			
						if (NearestOutcome == EGeometryScriptSearchOutcomePins::Found)
						{
							// Triangle ID를 통해 UV 가져오기 (UV Channel 0 기준)
							bool bIsValidTriangle = false;
							FVector2D UV;
							UGeometryScriptLibrary_MeshQueryFunctions::GetInterpolatedTriangleUV(
								DynamicMesh, 0, NearestResult.TriangleID, NearestResult.BaryCoords, bIsValidTriangle, UV);
			
							// 배열에 U, V 값 기록 (1차원 인덱스로 변환)
							int32 Index = X + (Y * Resolution) + (Z * Resolution * Resolution);
							VoxelData[Index] = FFloat16Color(FLinearColor(UV.X, UV.Y, 0.0f, 1.0f));
						}
					}
				}
			}
#if WITH_EDITORONLY_DATA
			UVVolumeTextures[i]->Source.Init(
				Resolution,
				Resolution,
				Resolution,
				1,
				TSF_RGBA16F,
				(const uint8*)VoxelData.GetData());
			
			UVVolumeTextures[i]->SRGB = false;
			UVVolumeTextures[i]->MipGenSettings = TMGS_NoMipmaps;
			UVVolumeTextures[i]->CompressionNone = true;
			UVVolumeTextures[i]->Filter = TF_Trilinear;
			UVVolumeTextures[i]->CompressionSettings = TC_Default;
#endif	
			UVVolumeTextures[i]->UpdateResource();
		}

		MorphingMeshData->UpdateBox();
	}
}
