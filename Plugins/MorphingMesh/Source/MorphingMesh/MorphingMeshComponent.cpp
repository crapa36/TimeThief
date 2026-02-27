// Fill out your copyright notice in the Description page of Project Settings.


#include "MorphingMeshComponent.h"
#include "MorphingMeshData.h"
#include "Core/LiquidMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
UStaticMeshComponent;
// Sets default values for this component's properties
UMorphingMeshComponent::UMorphingMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	if (MorphingMeshData == nullptr)
	{
		static ConstructorHelpers::FObjectFinder<UMorphingMeshData> Default(
			TEXT("/Script/MorphingMesh.MorphingMeshData'/MorphingMesh/Data/DA_MorphingData.DA_MorphingData'"));
		if (Default.Succeeded())
		{
			MorphingMeshData = Default.Object;
		}
	}
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	LiquidMeshComponent = CreateDefaultSubobject<ULiquidMeshComponent>("LiquidMeshComponent");
	LiquidMeshComponent->SetupAttachment(this);
	
	BaseMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("BaseMeshComponent");
	BaseMeshComponent->SetupAttachment(this);
	BaseMeshComponent->SetVisibility(false);
}


// Called when the game starts
void UMorphingMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	if (MorphingMeshData->IsValid())
	{
		BaseMeshComponent->SetStaticMesh(MorphingMeshData->GetBaseMeshes()[0]);
	}
	
	if (UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(LiquidMaterial, this))
	{
		for (int i = 0; i < 3; ++i)
		{
			DynMaterial->SetVectorParameterValue(FName{FString::Printf(TEXT("Min %d"), i + 1)}, MorphingMeshData->GetBounds()[i].Min);
			DynMaterial->SetVectorParameterValue(FName{FString::Printf(TEXT("Size %d"), i + 1)}, MorphingMeshData->GetBounds()[i].GetSize());
			DynMaterial->SetTextureParameterValue(FName{FString::Printf(TEXT("UV Volume Texture %d"), i + 1)}, MorphingMeshData->GetUVVolumeTextures()[i]);
		}
		
		DynMaterial->SetVectorParameterValue(FName{"Alpha"}, FVector{1,0,0});
		LiquidMaterial = DynMaterial;
		
		LiquidMeshComponent->SetMaterial(0, LiquidMaterial);
	}
}

void UMorphingMeshComponent::OnRegister()
{
	Super::OnRegister();
}

// Called every frame
void UMorphingMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (ElapsedTime >= MorphingTime)
	{
		CurrAlpha = DestAlpha;
	
		BaseMeshComponent->SetVisibility(true);
		LiquidMeshComponent->bRenderingEnable = false;
		
		SetComponentTickEnabled(false);
	}
	else
	{
		ElapsedTime += DeltaTime;
		const float AlphaRatio = FMath::Min(ElapsedTime / MorphingTime, 1.0f);
		CurrAlpha = FMath::Lerp(PrevAlpha, DestAlpha, AlphaRatio);
		
		if (UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(LiquidMaterial))
		{
			DynMaterial->SetVectorParameterValue(FName{"Alpha"}, CurrAlpha);
		}
	}
}

void UMorphingMeshComponent::SetType(EMorphTargetType NewType)
{
	if (MeshType == NewType)
	{
		return;
	}
	

	MeshType = NewType;
	
	int Index = static_cast<int>(MeshType);
	
	DestAlpha = FVector3f::ZeroVector;
	DestAlpha[Index] = 1.0f;
	
	PrevAlpha = CurrAlpha;
	
	MorphingTime = MaxMorphingTime * (1 - CurrAlpha[Index]);
	ElapsedTime = 0;
	
	BaseMeshComponent->SetStaticMesh(MorphingMeshData->GetBaseMeshes()[Index]);
	
	BaseMeshComponent->SetVisibility(false);
	LiquidMeshComponent->bRenderingEnable = true;
	
	SetComponentTickEnabled(true);
}
