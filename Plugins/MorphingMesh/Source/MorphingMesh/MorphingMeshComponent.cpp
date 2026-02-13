// Fill out your copyright notice in the Description page of Project Settings.


#include "MorphingMeshComponent.h"
#include "MorphingMeshData.h"
#include "Core/LiquidMeshComponent.h"
#include "GameFramework/Pawn.h"


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
	LiquidMeshComponent->SetVisibility(false);
	
	BaseMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("BaseMeshComponent");
	BaseMeshComponent->SetupAttachment(this);
	BaseMeshComponent->SetVisibility(true);
}


// Called when the game starts
void UMorphingMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	if (MorphingMeshData->IsValid())
	{
		BaseMeshComponent->SetStaticMesh(MorphingMeshData->GetBaseMeshes()[0]);
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
	
	if (Alpha.Equals(DestAlpha))
	{
		Alpha = DestAlpha;
		ElapsedTime = 0;
	
		BaseMeshComponent->SetVisibility(true);
		LiquidMeshComponent->SetVisibility(false);
		
		SetComponentTickEnabled(false);
	}
	else
	{
		ElapsedTime += DeltaTime;
		const float AlphaRatio = FMath::Clamp(ElapsedTime / MorphingTime, 0.0f, 1.0f);
		Alpha = FMath::Lerp(Alpha, DestAlpha, AlphaRatio);
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
	ElapsedTime = Alpha[Index] * MorphingTime;
	BaseMeshComponent->SetStaticMesh(MorphingMeshData->GetBaseMeshes()[Index]);
	
	BaseMeshComponent->SetVisibility(false);
	LiquidMeshComponent->SetVisibility(true);
	
	SetComponentTickEnabled(true);
}
