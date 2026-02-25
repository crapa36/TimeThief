// Fill out your copyright notice in the Description page of Project Settings.


#include "LiquidMeshComponent.h"
#include "LiquidMeshProxy.h"
#include "MorphingMeshSubsystem.h"
#include "MorphingMeshViewExtension.h"
#include "../MorphingMeshComponent.h"
#include "MorphingMesh/MorphingMeshData.h"
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
	if (ParentComponent == nullptr || !ParentComponent->MorphingMeshData->IsValid())
	{
		return;
	}
	if (FLiquidMeshProxy* Proxy = static_cast<FLiquidMeshProxy*>(SceneProxy))
	{
		Proxy->CachingData();
	}
	UpdateBounds();
}

void ULiquidMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
	if (FLiquidMeshProxy* Proxy = static_cast<FLiquidMeshProxy*>(SceneProxy))
	{
		ENQUEUE_RENDER_COMMAND(SetMaterial)([InMaterial, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			Proxy->SetMaterial(InMaterial);
		});
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
	if (UMorphingMeshSubsystem* Subsystem = GetWorld()->GetSubsystem<UMorphingMeshSubsystem>())
	{
		if (Subsystem->ViewExtension.IsValid())
		{
			Subsystem->ViewExtension->AddProxy(Proxy);
		}
	}
	return Proxy;
}

void ULiquidMeshComponent::DestroyRenderState_Concurrent()
{
	if (UMorphingMeshSubsystem* Subsystem = GetWorld()->GetSubsystem<UMorphingMeshSubsystem>())
	{
		if (Subsystem->ViewExtension.IsValid())
		{
			if (FLiquidMeshProxy* Proxy = static_cast<FLiquidMeshProxy*>(SceneProxy))
			{
				Subsystem->ViewExtension->RemoveProxy(Proxy);
			}
		}
	}

	Super::DestroyRenderState_Concurrent();
}

FBox ULiquidMeshComponent::GetBound() const
{
	if (!ParentComponent || !IsValid(ParentComponent))
	{
		return FBox(ForceInit);
	}

	const UMorphingMeshData* Data = ParentComponent->MorphingMeshData;
	if (!Data || !IsValid(Data) || !Data->IsValid())
	{
		return FBox(ForceInit);
	}

	const FVector3f& Alpha = ParentComponent->Alpha;
	const TArray<FBox>& Bound = Data->GetBounds();

	// Bounds는 3개라는 전제를 깰 수 있으니 방어
	if (Bound.Num() < 3)
	{
		return Bound.Num() > 0 ? Bound[0] : FBox(ForceInit);
	}

	FBox ResultBox(ForceInitToZero);
	for (int32 i = 0; i < 3; ++i)
	{
		ResultBox.Min += Bound[i].Min * Alpha[i];
		ResultBox.Max += Bound[i].Max * Alpha[i];
	}
	return ResultBox;
}

FVector3f ULiquidMeshComponent::GetAlpha() const
{
	return ParentComponent ? ParentComponent->Alpha : FVector3f::ZeroVector;
}

TArray<TObjectPtr<UVolumeTexture>> ULiquidMeshComponent::GetDensityTextures() const
{
	if (ParentComponent == nullptr || ParentComponent->MorphingMeshData == nullptr || !ParentComponent->MorphingMeshData
		->IsValid())
	{
		return TArray<TObjectPtr<UVolumeTexture>>{};
	}
	if (IsPlayerControlled())
	{
		return ParentComponent->MorphingMeshData->GetDensityTextures(EVoxelResolution::High);
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

		float Dist = FVector::Dist(CameraLocation, GetComponentLocation());
		if (Dist > 7500)
		{
			return TArray<TObjectPtr<UVolumeTexture>>{};
		}
		if (Dist > 1000)
		{
			return ParentComponent->MorphingMeshData->GetDensityTextures(EVoxelResolution::Low);
		}

		return ParentComponent->MorphingMeshData->GetDensityTextures(EVoxelResolution::Middle);
	}

	return TArray<TObjectPtr<UVolumeTexture>>{};
}
