// Fill out your copyright notice in the Description page of Project Settings.


#include "MorphingMeshComponent.h"
#include "MorphingMeshData.h"
#include "Core/LiquidMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	LiquidMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	BaseMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("BaseMeshComponent");
	BaseMeshComponent->SetupAttachment(this);
	BaseMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BaseMeshComponent->SetStaticMesh(nullptr);
	BaseMeshComponent->SetVisibility(false);
}


// Called when the game starts
void UMorphingMeshComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(LiquidMaterial, this))
	{
		for (int i = 0; i < 3; ++i)
		{
			DynMaterial->SetVectorParameterValue(FName{FString::Printf(TEXT("Min %d"), i + 1)}, MorphingMeshData->GetBounds()[i].Min);
			DynMaterial->SetVectorParameterValue(FName{FString::Printf(TEXT("Size %d"), i + 1)}, MorphingMeshData->GetBounds()[i].GetSize());
			DynMaterial->SetTextureParameterValue(FName{FString::Printf(TEXT("UV Volume Texture %d"), i + 1)}, MorphingMeshData->GetUVVolumeTextures()[i]);
			
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("UV Volume Texture %d"), i + 1));
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
	
		// BaseMeshComponent->SetVisibility(true);
		// LiquidMeshComponent->bRenderingEnable = false;
		
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
	
	ElapsedTime = 0;
	int Index = static_cast<int>(NewType);
	DestAlpha = FVector3f::ZeroVector;
	
	if (NewType == EMorphTargetType::None)
	{
		MorphingTime = MaxMorphingTime;
		BaseMeshComponent->SetStaticMesh(nullptr);
	}
	else
	{
		DestAlpha[Index] = 1.0f;
		
		MorphingTime = MaxMorphingTime * (1 - CurrAlpha[Index]);
		BaseMeshComponent->SetStaticMesh(MorphingMeshData->GetBaseMeshes()[Index]);
	}
	
	MeshType = NewType;
	PrevAlpha = CurrAlpha;
	
	BaseMeshComponent->SetVisibility(false);
	LiquidMeshComponent->bRenderingEnable = true;
	
	SetComponentTickEnabled(true);
}
