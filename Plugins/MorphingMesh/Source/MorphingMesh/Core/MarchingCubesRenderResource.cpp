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
	const FVector3f& Alpha,
	const TArray<TObjectPtr<UVolumeTexture>>& VolumeTextures,
	const TArray<TObjectPtr<UVolumeTexture>>& UVMaps)
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
	
	TShaderMapRef<FBlockScan> BlockScanShader(GetGlobalShaderMap(GetFeatureLevel()));
	FBlockScan::FParameters* BlockScanParams = GraphBuilder.AllocParameters<FBlockScan::FParameters>();
	
	FRDGBufferRef ScanBufferRDG = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumVoxels),
		TEXT("BlockScanBuffer"));
	
	FRDGBufferRef OffsetRDG = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), GroupCount + 1),
		TEXT("OffsetBuffer"));

	
	BlockScanParams->InputBuffer = GraphBuilder.CreateSRV(TriCountRDG);
	BlockScanParams->BlockScanBuffer = GraphBuilder.CreateUAV(ScanBufferRDG);
	BlockScanParams->OffsetBuffer = GraphBuilder.CreateUAV(OffsetRDG);
	
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MC_PrefixSum"),
		ERDGPassFlags::Compute,
		BlockScanShader,
		BlockScanParams,
		FIntVector(GroupCount, 1, 1));
	
	TShaderMapRef<FAddOffset> AddOffsetShader(GetGlobalShaderMap(GetFeatureLevel()));
	FAddOffset::FParameters* AddOffsetParameters = GraphBuilder.AllocParameters<FAddOffset::FParameters>();
	AddOffsetParameters->OffsetBuffer = GraphBuilder.CreateUAV(OffsetRDG);
	AddOffsetParameters->OffsetBufferSize = GroupCount;
	
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("AddOffsetCS"),
		ERDGPassFlags::Compute,
		AddOffsetShader,
		AddOffsetParameters,
		FIntVector(1, 1, 1));
	
	
	TShaderMapRef<FEmit> EmitShader(GetGlobalShaderMap(GetFeatureLevel()));
	FEmit::FParameters* EmitParams = GraphBuilder.AllocParameters<FEmit::FParameters>();
	EmitParams->Constants = ConstBufferRDG;
	EmitParams->CubeCase = GraphBuilder.CreateSRV(CubeCaseRDG);
	EmitParams->TriCount = GraphBuilder.CreateSRV(TriCountRDG);
	EmitParams->OffsetBuffer = GraphBuilder.CreateSRV(OffsetRDG);
	EmitParams->PrefixBuffer = GraphBuilder.CreateSRV(ScanBufferRDG);
	EmitParams->PositionBuffer = GraphBuilder.CreateUAV(PositionRDG);
	EmitParams->TangentsBuffer = GraphBuilder.CreateUAV(TangentsRDG);
	EmitParams->IndirectArgsBuffer = GraphBuilder.CreateUAV(IndirectArgsRDG);
	EmitParams->Density0 = VolumeTextures[0]->GetResource()->GetTexture3DRHI();
	EmitParams->Density1 = VolumeTextures[1]->GetResource()->GetTexture3DRHI();
	EmitParams->Density2 = VolumeTextures[2]->GetResource()->GetTexture3DRHI();
	EmitParams->UVMap = UVMaps[0]->GetResource()->GetTexture3DRHI();
	EmitParams->UVMapSampler = TStaticSamplerState<>::GetRHI();
	EmitParams->UVBuffer = GraphBuilder.CreateUAV(UVRDG);
	
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MC_Emit"),
		ERDGPassFlags::Compute,
		EmitShader,
		EmitParams,
		FIntVector(GroupCount, 1, 1));
	
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
		
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(float2), NumVertex);
		
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

		Data.NumTexCoords = 1;
		
		Data.TextureCoordinates.Emplace(FVertexStreamComponent(
			UVBuffer.Get(),
			0,
			sizeof(float2),
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
