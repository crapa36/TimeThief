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
#include "Rendering/SkeletalMeshRenderData.h"
#include "Async/ParallelFor.h"

struct FBoneSegment
{
	FVector Start;
	FVector End;
	int32 BoneIndex;
};

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

void ADataSetActor::CreateBoneIndexTexture() const
{
    const TArray<TObjectPtr<UStaticMesh>>& BaseMeshes = MorphingMeshData->GetBaseMeshes();

    for (int32 i = 0; i < MorphingMeshData->BoneIndexTextures.Num(); ++i)
    {
       UVolumeTexture* VolumeTex = MorphingMeshData->BoneIndexTextures[i];
       USkeletalMesh* SkelMesh = MorphingMeshData->SkeletalMeshes.IsValidIndex(i)
          ? MorphingMeshData->SkeletalMeshes[i]
          : nullptr;
       UStaticMesh* BaseMesh = BaseMeshes.IsValidIndex(i) ? BaseMeshes[i] : nullptr;

       if (!VolumeTex || !SkelMesh || !BaseMesh)
       {
          continue;
       }

       // --- 1. 스켈레탈 메쉬의 렌더 데이터(표면 및 웨이트) 가져오기 ---
       FSkeletalMeshRenderData* RenderData = SkelMesh->GetResourceForRendering();
       if (!RenderData || RenderData->LODRenderData.IsEmpty())
       {
           UE_LOG(LogTemp, Warning, TEXT("SkeletalMesh %s has no render data!"), *SkelMesh->GetName());
           continue;
       }

       const FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[0];
       const FPositionVertexBuffer& PositionBuffer = LODData.StaticVertexBuffers.PositionVertexBuffer;
       const FSkinWeightVertexBuffer& SkinWeightBuffer = LODData.SkinWeightVertexBuffer;

       struct FSurfacePoint
       {
           FVector Position;
           uint8 DominantBoneIndex;
       };
       TArray<FSurfacePoint> SurfacePoints;
       SurfacePoints.Reserve(PositionBuffer.GetNumVertices());

       // --- 2. 모든 버텍스를 순회하며 가장 높은 웨이트를 가진 본 인덱스 추출 ---
       for (int32 SecIdx = 0; SecIdx < LODData.RenderSections.Num(); ++SecIdx)
       {
           const FSkelMeshRenderSection& Section = LODData.RenderSections[SecIdx];
           for (int32 VertIdx = 0; VertIdx < Section.GetNumVertices(); ++VertIdx)
           {
               uint32 GlobalVertIdx = Section.BaseVertexIndex + VertIdx;
               
               // 버텍스의 로컬 위치 (UE5 기준 FVector3f를 FVector로 캐스팅)
               FVector Pos = (FVector)PositionBuffer.VertexPosition(GlobalVertIdx);

               int32 MaxWeight = -1;
               uint8 BestBone = 0;

               // 해당 버텍스에 영향을 주는 본들 중 가중치(Weight)가 가장 큰 본 찾기
               for (uint32 InfIdx = 0; InfIdx < SkinWeightBuffer.GetMaxBoneInfluences(); ++InfIdx)
               {
                   int32 BoneWeight = SkinWeightBuffer.GetBoneWeight(GlobalVertIdx, InfIdx);
                   if (BoneWeight > MaxWeight)
                   {
                       MaxWeight = BoneWeight;
                       // Section.BoneMap을 통해 실제 글로벌 본 인덱스를 얻어옴
                       uint32 SectionBoneIdx = SkinWeightBuffer.GetBoneIndex(GlobalVertIdx, InfIdx);
                       BestBone = static_cast<uint8>(Section.BoneMap[SectionBoneIdx]);
                   }
               }

               SurfacePoints.Add({ Pos, BestBone });
           }
       }

       if (SurfacePoints.IsEmpty())
       {
           continue;
       }

       // --- 3. 바운딩 박스 설정 ---
       const FBox Bounds = BaseMesh->GetBoundingBox(); 
       const FVector Min = Bounds.Min;
       const FVector Size = Bounds.GetSize();
       
       int BoneResolution = Resolution;
       TArray<uint8> VoxelBoneIndices;
       int32 TotalVoxels = BoneResolution * BoneResolution * BoneResolution;
       VoxelBoneIndices.SetNumZeroed(TotalVoxels);

       const float InvResMinus1 = 1.0f / float(BoneResolution - 1);

       // --- 4. 복셀 그리드 채우기 (속도를 위해 ParallelFor 사용) ---
       // 각 복셀 위치에서 가장 가까운 버텍스(표면)를 찾아 해당 버텍스의 본 인덱스를 저장
       ParallelFor(TotalVoxels, [&](int32 FlatIndex)
       {
           int32 X = FlatIndex % BoneResolution;
           int32 Y = (FlatIndex / BoneResolution) % BoneResolution;
           int32 Z = FlatIndex / (BoneResolution * BoneResolution);

           const float AlphaX = X * InvResMinus1;
           const float AlphaY = Y * InvResMinus1;
           const float AlphaZ = Z * InvResMinus1;

           const FVector VoxelPos = Min + FVector(AlphaX, AlphaY, AlphaZ) * Size;

           float MinDistanceSq = MAX_FLT;
           uint8 ClosestBoneIndex = 0;

           // 가장 가까운 스켈레탈 메쉬 표면(버텍스) 찾기
           for (const FSurfacePoint& Pt : SurfacePoints)
           {
               const float DistSq = FVector::DistSquared(VoxelPos, Pt.Position);
               if (DistSq < MinDistanceSq)
               {
                   MinDistanceSq = DistSq;
                   ClosestBoneIndex = Pt.DominantBoneIndex;
               }
           }

           VoxelBoneIndices[FlatIndex] = ClosestBoneIndex;
       });

#if WITH_EDITORONLY_DATA
       VolumeTex->Source.Init(
          BoneResolution,
          BoneResolution,
          BoneResolution,
          1,
          TSF_G8,
          VoxelBoneIndices.GetData());

       VolumeTex->SRGB = false;
       VolumeTex->MipGenSettings = TMGS_NoMipmaps;
       VolumeTex->CompressionNone = true;
       VolumeTex->Filter = TF_Nearest;
       VolumeTex->NeverStream = true;
       VolumeTex->CompressionSettings = TC_Grayscale;
       VolumeTex->PostEditChange();
#endif
       VolumeTex->UpdateResource();
    }
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
		const TArray<TObjectPtr<UVolumeTexture>>& UVVolumeTextures = MorphingMeshData->GetUVVolumeTextures();
		MorphingMeshData->UpdateBox();

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
								DynamicMesh, 0, NearestResult.TriangleID, NearestResult.BaryCoords, bIsValidTriangle,
								UV);

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
			UVVolumeTextures[i]->NeverStream = true;
#endif
			UVVolumeTextures[i]->UpdateResource();
		}
		
		CreateBoneIndexTexture();
	}
}
