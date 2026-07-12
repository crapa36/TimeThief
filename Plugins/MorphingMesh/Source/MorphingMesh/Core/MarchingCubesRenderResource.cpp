// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingCubesRenderResource.h"
#include "RenderGraphBuilder.h"
#include "HLSLTypeAliases.h"
#include "RenderGraphUtils.h"
#include "CS_Library.h"
#include "Engine/VolumeTexture.h"

void FMarchingCubesRenderResource::FVertexBufferWithRDG::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (Buffer.IsValid())
	{
		VertexBufferRHI = Buffer->GetRHI();
	}
	ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(
		VertexBufferRHI,
		FRHIViewDesc::CreateBufferSRV()
		.SetType(FRHIViewDesc::EBufferType::Structured));
}

void FMarchingCubesRenderResource::FVertexBufferWithRDG::ReleaseRHI()
{
	FVertexBuffer::ReleaseRHI();
	Buffer.SafeRelease();
}

FMarchingCubesRenderResource::FMarchingCubesRenderResource(
	ERHIFeatureLevel::Type InFeatureLevel,
	const FIntVector3& InDimensions)
	: FRenderResource{InFeatureLevel}
	  , VertexFactory{InFeatureLevel, "FMarchingCubesRenderResource"}
	  , NumVertex{InDimensions.X * InDimensions.Y * InDimensions.Z * 5 * 3}
{
}

void FMarchingCubesRenderResource::RunComputeShader(
	FRDGBuilder& GraphBuilder,
	const FBox& InBound,
	const FVector& Alpha,
	const TArray<TObjectPtr<UVolumeTexture>>& VolumeTextures,
	const TArray<TObjectPtr<UVolumeTexture>>& UVMaps,
	TObjectPtr<UVolumeTexture> BoneIndicesTexture,
	TObjectPtr<UVolumeTexture> BoneWeightsTexture,
	const TArray<FMatrix44f>& SkinMatrices)
{
	using namespace UE::HLSL;
	
	if (VolumeTextures.Num() == 0)
	{
		return;
	}
	
	FRDGBufferRef PositionRDG = GraphBuilder.RegisterExternalBuffer(PositionBuffer->Buffer);
	FRDGBufferRef TangentsRDG = GraphBuilder.RegisterExternalBuffer(TangentsBuffer->Buffer);
	FRDGBufferRef IndirectArgsRDG = GraphBuilder.RegisterExternalBuffer(IndirectArgsBuffer);
	FRDGBufferRef UVRDG = GraphBuilder.RegisterExternalBuffer(UVBuffer->Buffer);
	
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(IndirectArgsRDG), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PositionRDG), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TangentsRDG), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(UVRDG), 0);
	
	FConstBuffer* ConstBufferData = GraphBuilder.AllocParameters<FConstBuffer>();
	int NumVoxels = VolumeTextures[0]->GetSizeX() *
	                  VolumeTextures[0]->GetSizeY() *
	                  VolumeTextures[0]->GetSizeZ();
	
	ConstBufferData->GridSize = uint3(VolumeTextures[0]->GetSizeX());
	ConstBufferData->IsoLevel = 0.0f;
	ConstBufferData->BoxMin = float3(InBound.Min);
	ConstBufferData->NumVoxels = NumVoxels;
	ConstBufferData->VoxelSize = float3(InBound.GetSize()) / float3(VolumeTextures[0]->GetSizeX() - 1) ;
	ConstBufferData->Alpha = float3(Alpha);
	ConstBufferData->bApplySkin = BoneIndicesTexture ? 1 : 0;
	TUniformBufferRef<FConstBuffer> ConstBufferRDG = TUniformBufferRef<FConstBuffer>::CreateUniformBufferImmediate(*ConstBufferData, UniformBuffer_SingleFrame);
	int GroupCount = NumVoxels / 256;

	TShaderMapRef<FClassify> ClassifyShader(GetGlobalShaderMap(GetFeatureLevel()));
	FClassify::FParameters* ClassifyParams = GraphBuilder.AllocParameters<FClassify::FParameters>();
	
	FRDGBufferRef TriCountRDG = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumVoxels),
		TEXT("TriCountBuffer"));
	FRDGBufferRef CubeCaseRDG = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumVoxels),
		TEXT("CubeCaseBuffer"));
	
	ClassifyParams->Constants = ConstBufferRDG;
	ClassifyParams->TriCount = GraphBuilder.CreateUAV(TriCountRDG);
	ClassifyParams->CubeCase = GraphBuilder.CreateUAV(CubeCaseRDG);
	ClassifyParams->Density0 = VolumeTextures[0]->GetResource()->GetTexture3DRHI();
	ClassifyParams->Density1 = VolumeTextures[1]->GetResource()->GetTexture3DRHI();
	ClassifyParams->Density2 = VolumeTextures[2]->GetResource()->GetTexture3DRHI();
	
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MC_Classify"),
		ERDGPassFlags::Compute,
		ClassifyShader,
		ClassifyParams,
		FIntVector(GroupCount, 1, 1));
	

	
	FRDGBufferRef ScanBufferRDG = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumVoxels),
		TEXT("BlockScanBuffer"));
	
	FRDGBufferRef IndexRDG = GraphBuilder.CreateBuffer(
	FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
	TEXT("IndexBuffer"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(IndexRDG), 0);
	
	FRDGBufferRef StateBufferRDG = GraphBuilder.CreateBuffer(
	FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), GroupCount * 3),
	TEXT("StateBuffer"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(StateBufferRDG), 0);
	
	TShaderMapRef<FDecoupledScan> DecoupledScanShader(GetGlobalShaderMap(GetFeatureLevel()));
	FDecoupledScan::FParameters* DecoupledScanParams = GraphBuilder.AllocParameters<FDecoupledScan::FParameters>();
	DecoupledScanParams->GlobalPrefixBuffer = GraphBuilder.CreateUAV(ScanBufferRDG);
	DecoupledScanParams->InputBuffer = GraphBuilder.CreateSRV(TriCountRDG);
	DecoupledScanParams->IndexBuffer = GraphBuilder.CreateUAV(IndexRDG);
	DecoupledScanParams->StateBuffer = GraphBuilder.CreateUAV(StateBufferRDG);
	DecoupledScanParams->NumVoxels = NumVoxels;
	FComputeShaderUtils::AddPass(
	GraphBuilder,
		RDG_EVENT_NAME("DecoupledScan"),
	ERDGPassFlags::Compute,
	DecoupledScanShader,
	DecoupledScanParams,
	FIntVector(GroupCount, 1, 1)
	);
	
	TShaderMapRef<FEmit> EmitShader(GetGlobalShaderMap(GetFeatureLevel()));
	FEmit::FParameters* EmitParams = GraphBuilder.AllocParameters<FEmit::FParameters>();
	EmitParams->Constants = ConstBufferRDG;
	EmitParams->CubeCase = GraphBuilder.CreateSRV(CubeCaseRDG);
	EmitParams->TriCount = GraphBuilder.CreateSRV(TriCountRDG);
	EmitParams->PrefixBuffer = GraphBuilder.CreateSRV(ScanBufferRDG);
	EmitParams->PositionBuffer = GraphBuilder.CreateUAV(PositionRDG);
	EmitParams->TangentsBuffer = GraphBuilder.CreateUAV(TangentsRDG);
	EmitParams->IndirectArgsBuffer = GraphBuilder.CreateUAV(IndirectArgsRDG);
	EmitParams->Density0 = VolumeTextures[0]->GetResource()->GetTexture3DRHI();
	EmitParams->Density1 = VolumeTextures[1]->GetResource()->GetTexture3DRHI();
	EmitParams->Density2 = VolumeTextures[2]->GetResource()->GetTexture3DRHI();
	EmitParams->UVMap0 = UVMaps[0]->GetResource()->GetTexture3DRHI();
	EmitParams->UVMap1 = UVMaps[1]->GetResource()->GetTexture3DRHI();
	EmitParams->UVMap2 = UVMaps[2]->GetResource()->GetTexture3DRHI();
	EmitParams->UVMapSampler = TStaticSamplerState<SF_Trilinear>::GetRHI();
	EmitParams->UVBuffer = GraphBuilder.CreateUAV(UVRDG);
	EmitParams->BoneIndicesSampler = TStaticSamplerState<>::GetRHI();
	EmitParams->BoneIndicesTexture = BoneIndicesTexture ? BoneIndicesTexture->GetResource()->GetTexture3DRHI() : VolumeTextures[0]->GetResource()->GetTexture3DRHI();
	EmitParams->BoneWeightsSampler = TStaticSamplerState<>::GetRHI();
	EmitParams->BoneWeightsTexture = BoneWeightsTexture ? BoneWeightsTexture->GetResource()->GetTexture3DRHI() : VolumeTextures[0]->GetResource()->GetTexture3DRHI();
	
	FRDGBufferRef BoneIndicesBuffer = GraphBuilder.CreateBuffer(
	FRDGBufferDesc::CreateStructuredDesc(sizeof(float4), NumVertex),
	TEXT("BoneIndicesBuffer"));
	EmitParams->BoneIndicesBuffer = GraphBuilder.CreateUAV(BoneIndicesBuffer);
	
	FRDGBufferRef BoneWeightsBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(float4), NumVertex),	
		TEXT("BoneWeightsBuffer"));
	EmitParams->BoneWeightsBuffer = GraphBuilder.CreateUAV(BoneWeightsBuffer);
	
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MC_Emit"),
		ERDGPassFlags::Compute,
		EmitShader,
		EmitParams,
		FIntVector(GroupCount, 1, 1));
	
	if (BoneIndicesTexture && SkinMatrices.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rigging"));
		FRDGBufferRef BoneMatrixRDG = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FMatrix44f), SkinMatrices.Num()),
			TEXT("BoneMatrixBuffer"));
		
		GraphBuilder.QueueBufferUpload(
		BoneMatrixRDG,
		SkinMatrices.GetData(),
		sizeof(FMatrix44f) * SkinMatrices.Num()
		);
		
		TShaderMapRef<FSkin> SkinShader(GetGlobalShaderMap(GetFeatureLevel()));
		FSkin::FParameters* SkinParams = GraphBuilder.AllocParameters<FSkin::FParameters>();
		SkinParams->PositionBuffer = GraphBuilder.CreateUAV(PositionRDG);
		SkinParams->TangentsBuffer = GraphBuilder.CreateUAV(TangentsRDG);
		SkinParams->BoneMatrices = GraphBuilder.CreateSRV(BoneMatrixRDG);
		SkinParams->BoneIndicesBuffer = GraphBuilder.CreateSRV(BoneIndicesBuffer);
		SkinParams->BoneWeightsBuffer = GraphBuilder.CreateSRV(BoneWeightsBuffer);
		SkinParams->IndirectArgsBuffer = GraphBuilder.CreateSRV(IndirectArgsRDG);
		SkinParams->NumMatrix = SkinMatrices.Num();
		const int32 SkinGroupCount = FMath::DivideAndRoundUp(NumVertex, 256);
		
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Skin"),
			ERDGPassFlags::Compute,
			SkinShader,
			SkinParams,
			FIntVector(SkinGroupCount, 1, 1));
	}
	
	GraphBuilder.UseExternalAccessMode(PositionRDG, ERHIAccess::VertexOrIndexBuffer);
	GraphBuilder.UseExternalAccessMode(TangentsRDG, ERHIAccess::VertexOrIndexBuffer);
	GraphBuilder.UseExternalAccessMode(IndirectArgsRDG, ERHIAccess::IndirectArgs);
	GraphBuilder.UseExternalAccessMode(UVRDG, ERHIAccess::VertexOrIndexBuffer);
}

void FMarchingCubesRenderResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	using namespace UE::HLSL;
	// NumVertex가 0이면 버퍼를 할당하지 않음
	if (NumVertex <= 0)
	{
		return;
	}
	
	FRDGBuilder GraphBuilder{static_cast<FRHICommandListImmediate&>(RHICmdList)};

	// Create Position Buffer - RDG PooledBuffer 사용
	{
		PositionBuffer = MakeUnique<FVertexBufferWithRDG>();
		
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(float3), NumVertex);
		
		FRDGBufferRef RDGBuffer = GraphBuilder.CreateBuffer(Desc,TEXT("PositionBuffer"));
		
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RDGBuffer), 0);
		
		GraphBuilder.QueueBufferExtraction(RDGBuffer, &PositionBuffer->Buffer);
	}

	// Create Tangents Buffer - RDG PooledBuffer 사용
	{
		TangentsBuffer = MakeUnique<FVertexBufferWithRDG>();
		
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(float3), NumVertex * 2);
		
		FRDGBufferRef RDGBuffer = GraphBuilder.CreateBuffer(Desc,TEXT("TangentsBuffer"));
		
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RDGBuffer), 0);
		
		GraphBuilder.QueueBufferExtraction(RDGBuffer, &TangentsBuffer->Buffer);
	}

	// Create Indirect Args Buffer
	{
		
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateIndirectDesc(sizeof(uint32), 5);
		
		FRDGBufferRef RDGBuffer = GraphBuilder.CreateBuffer(Desc,TEXT("IndirectArgsBuffer"));
		
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RDGBuffer), 0);
		
		GraphBuilder.QueueBufferExtraction(RDGBuffer, &IndirectArgsBuffer);
	}
	
	// Create UV Buffer - RDG PooledBuffer 사용
	{
		UVBuffer = MakeUnique<FVertexBufferWithRDG>();
		
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(float2), NumVertex * 3);
		
		FRDGBufferRef RDGBuffer = GraphBuilder.CreateBuffer(Desc,TEXT("UVBuffer"));
		
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(RDGBuffer), 0);
		
		GraphBuilder.QueueBufferExtraction(RDGBuffer, &UVBuffer->Buffer);
	}
	
	GraphBuilder.Execute();
	PositionBuffer->InitResource(RHICmdList);
	TangentsBuffer->InitResource(RHICmdList);
	UVBuffer->InitResource(RHICmdList);
	
	// Initialize Vertex Factory
	{
		FLocalVertexFactory::FDataType Data;

		Data.PositionComponent = FVertexStreamComponent(
			PositionBuffer.Get(),
			0,
			sizeof(float3),
			VET_Float3
		);

		Data.PositionComponentSRV = PositionBuffer->ShaderResourceViewRHI;

		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			TangentsBuffer.Get(),
			0,
			sizeof(float3) * 2,
			VET_Float3
		);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			TangentsBuffer.Get(),
			sizeof(float3),
			sizeof(float3) * 2,
			VET_Float3
		);
		Data.TangentsSRV = TangentsBuffer->ShaderResourceViewRHI;

		Data.NumTexCoords = 3;
		
		uint32 Stride = sizeof(float2) * Data.NumTexCoords;
		for (int i = 0; i < Data.NumTexCoords; ++i)
		Data.TextureCoordinates.Emplace(FVertexStreamComponent(
			UVBuffer.Get(),
			sizeof(float2) * i,
			Stride,
			VET_Float2)
		);
		Data.TextureCoordinatesSRV = UVBuffer->ShaderResourceViewRHI;
		// Data.TextureCoordinatesSRV = GNullColorVertexBuffer.VertexBufferSRV;
		Data.ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;

		VertexFactory.SetData(RHICmdList, Data);
		VertexFactory.InitResource(RHICmdList);
	}
}

void FMarchingCubesRenderResource::ReleaseRHI()
{
	if (PositionBuffer)
	{
		PositionBuffer->ReleaseResource();
		PositionBuffer.Reset();
	}

	if (TangentsBuffer)
	{
		TangentsBuffer->ReleaseResource();
		TangentsBuffer.Reset();
	}
	
	if (UVBuffer)
	{
		UVBuffer->ReleaseResource();
		UVBuffer.Reset();
	}
	IndirectArgsBuffer.SafeRelease();

	VertexFactory.ReleaseResource();
}
