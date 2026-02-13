// Fill out your copyright notice in the Description page of Project Settings.


#include "LiquidMeshComponent.h"
#include "LiquidMeshProxy.h"
#include "../MorphingMeshComponent.h"
#include "MorphingMesh/MorphingMeshData.h"
#include "RenderGraphBuilder.h"
#include "../Settings.h"
// Sets default values for this component's properties
ULiquidMeshComponent::ULiquidMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	PrimaryComponentTick.bCanEverTick = true;
	CastShadow = true;
	bCastDynamicShadow = true;
	bUseAsOccluder = true;
	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

// Called when the game starts
void ULiquidMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	// BeginPlay 시점에서 플레이어 여부 확인 및 캐시
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		bIsPlayerControlled = Pawn->IsPlayerControlled();
	}

	if (bIsPlayerControlled)
	{
		MarkRenderStateDirty();
	}

	const FBox Bound = GetBound();
	const FVector3f Alpha = ParentComponent->Alpha;
	TArray<TObjectPtr<UVolumeTexture>> DensityTextures;
	if (IsPlayerControlled())
	{
		DensityTextures = ParentComponent->MorphingMeshData->GetDensityTextures(EVoxelResolution::High);
	}
	else
	{
		DensityTextures = ParentComponent->MorphingMeshData->GetDensityTextures(EVoxelResolution::Middle);
	}

	if (FLiquidMeshProxy* Proxy = static_cast<FLiquidMeshProxy*>(GetSceneProxy()))
	{
		ENQUEUE_RENDER_COMMAND(Liquid)(
			[Bound, Alpha, DensityTextures, Proxy](FRHICommandListImmediate& RHICmdList)
			{
				FRDGBuilder GraphBuilder{RHICmdList};
				Proxy->UpdateRenderResource(GraphBuilder, Bound, Alpha, DensityTextures);
				GraphBuilder.Execute();
			}
		);
	}
}

void ULiquidMeshComponent::OnRegister()
{
	Super::OnRegister();

	if (const UMorphingMeshComponent* Component = Cast<UMorphingMeshComponent>(GetAttachParent()))
	{
		ParentComponent = Component;
	}
}


// Called every frame
void ULiquidMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateBounds();

	if (ParentComponent == nullptr || !ParentComponent->MorphingMeshData->IsValid())
	{
		return;
	}
	const FBox Bound = GetBound();
	const FVector3f Alpha = ParentComponent->Alpha;
	TArray<TObjectPtr<UVolumeTexture>> DensityTextures;

	if (IsPlayerControlled())
	{
		DensityTextures = ParentComponent->MorphingMeshData->GetDensityTextures(EVoxelResolution::High);
	}
	else
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

			float Dist = FVector::Dist(CameraLocation, GetComponentLocation());
			if (Dist > 7500)
			{
				return;
			}
			if (Dist > 1000)
			{
				DensityTextures = ParentComponent->MorphingMeshData->GetDensityTextures(EVoxelResolution::Low);
			}
			else
			{
				DensityTextures = ParentComponent->MorphingMeshData->GetDensityTextures(EVoxelResolution::Middle);
			}
		}
	}

	if (FLiquidMeshProxy* Proxy = static_cast<FLiquidMeshProxy*>(GetSceneProxy()))
	{
		ENQUEUE_RENDER_COMMAND(Liquid)(
			[Bound, Alpha, DensityTextures, Proxy](FRHICommandListImmediate& RHICmdList)
			{
				FRDGBuilder GraphBuilder{RHICmdList};
				Proxy->UpdateRenderResource(GraphBuilder, Bound, Alpha, DensityTextures);
				GraphBuilder.Execute();
			}
		);
	}
}

FBoxSphereBounds ULiquidMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(GetBound().ExpandBy(2)).TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* ULiquidMeshComponent::CreateSceneProxy()
{
	// DensityTextures와 Bounds가 유효하고, 데이터가 있을 때만 프록시 생성
	if (ParentComponent == nullptr || !ParentComponent->MorphingMeshData->IsValid())
	{
		return nullptr;
	}
	FLiquidMeshProxy* Proxy = new FLiquidMeshProxy(this);

	return Proxy;
}

FBox ULiquidMeshComponent::GetBound() const
{
	if (ParentComponent != nullptr && ParentComponent->MorphingMeshData->IsValid())
	{
		const FVector3f& Alpha = ParentComponent->Alpha;
		const TArray<FBox>& Bound = ParentComponent->MorphingMeshData->GetBounds();

		FBox ResultBox(ForceInitToZero);
		for (int i = 0; i < 3; ++i)
		{
			ResultBox.Min += Bound[i].Min * Alpha[i];
			ResultBox.Max += Bound[i].Max * Alpha[i];
		}
		return ResultBox;
	}
	return FBox(ForceInit);
}
