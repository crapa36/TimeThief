// Fill out your copyright notice in the Description page of Project Settings.


#include "MorphingMeshComponent.h"
#include "MorphingMeshData.h"
#include "Core/LiquidMeshComponent.h"
#include "GameFramework/Pawn.h"
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
	
	BaseSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("BaseSkeletalMeshComponent");
	BaseSkeletalMeshComponent->SetupAttachment(this);
	BaseSkeletalMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}


// Called when the game starts
void UMorphingMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(LiquidMaterial, this))
	{
		LiquidMaterial = DynMaterial;

		LiquidMeshComponent->SetMaterial(0, LiquidMaterial);
	}
}

void UMorphingMeshComponent::OnRegister()
{
	Super::OnRegister();
	
	if (MorphingMeshData == nullptr)
	{
		bIsSkeletalMesh = false;
		bIsValid = false;
		return;
	}
	
	bIsSkeletalMesh = MorphingMeshData->IsSkeletalValid();
	bIsValid = MorphingMeshData->IsValid();
	
	int Index = int(MeshType);
	PrevAlpha = FVector3f::ZeroVector;
	if (MeshType != EMorphTargetType::None)
	{
		
		PrevAlpha[Index] = 1.0f;
		if (bIsSkeletalMesh)
		{
			BaseSkeletalMeshComponent->EmptyOverrideMaterials();
			BaseSkeletalMeshComponent->SetSkeletalMesh(MorphingMeshData->SkeletalMeshes[Index]);
			BaseSkeletalMeshComponent->SetAnimInstanceClass(MorphingMeshData->AnimInstances[Index]);
			BaseMeshComponent->SetVisibility(false);
		}
		else
		{
			BaseMeshComponent->SetStaticMesh(MorphingMeshData->GetBaseMeshes()[Index]);
			BaseSkeletalMeshComponent->SetVisibility(false);
		}
	}
	
	CurrAlpha = PrevAlpha;
	DestAlpha = PrevAlpha;
}

// Called every frame
void UMorphingMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (MorphingMeshData == nullptr)
	{
		return;
	}
	if (ElapsedTime >= MorphingTime)
	{
		CurrAlpha = DestAlpha;
	
		if (!bDebug)
		{
			if (bIsSkeletalMesh)
			{
				BaseSkeletalMeshComponent->SetVisibility(true);
			}
			else
			{
				BaseMeshComponent->SetVisibility(true);
			}
			LiquidMeshComponent->bRenderingEnable = false;
		}
		SetComponentTickEnabled(false);
	}
	else
	{
		if (ElapsedTime >= MorphingTime / 10.f * 9.f || ElapsedTime <= MorphingTime / 10.f)
		{
			ElapsedTime -= DeltaTime / 10.f * 9.f;
		}
		else if (ElapsedTime >= MorphingTime / 10.f * 7.f || ElapsedTime <= MorphingTime / 10.f * 3.f)
		{
			ElapsedTime -= DeltaTime / 10.f * 7.f;
		}
		else if (ElapsedTime >= MorphingTime / 10.f * 4.f || ElapsedTime <= MorphingTime / 10.f * 6.f)
		{
			ElapsedTime += DeltaTime / 10.f * 5.f;
		}
		ElapsedTime += DeltaTime;
		const float AlphaRatio = FMath::Min(ElapsedTime / MorphingTime, 1.0f);
		CurrAlpha = FMath::Lerp(PrevAlpha, DestAlpha, AlphaRatio);
		if (bIsSkeletalMesh)
		{
			int Index = GetActiveSkeletalIndex();
			
			if (Index != -1)
			{
				BaseSkeletalMeshComponent->EmptyOverrideMaterials();
				BaseSkeletalMeshComponent->SetSkeletalMesh(MorphingMeshData->SkeletalMeshes[GetActiveSkeletalIndex()]);
				BaseSkeletalMeshComponent->SetAnimInstanceClass(MorphingMeshData->AnimInstances[GetActiveSkeletalIndex()]);
				UE_LOG(LogTemp, Warning, TEXT("SkeletalMesh %s"), *MorphingMeshData->SkeletalMeshes[GetActiveSkeletalIndex()]->GetName());
			}
			else
			{
				BaseSkeletalMeshComponent->SetSkeletalMeshAsset(nullptr);
			}
		}
		if (UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(LiquidMaterial))
		{
			DynMaterial->SetVectorParameterValue(FName{"Alpha"}, CurrAlpha);
		}
	}
}

int UMorphingMeshComponent::GetActiveSkeletalIndex() const
{
	if (CurrAlpha == FVector3f::ZeroVector)
	{
		return -1;
	}
	
	float DistX = FMath::Abs(CurrAlpha.GetMax() - CurrAlpha.X);
	float DistY = FMath::Abs(CurrAlpha.GetMax() - CurrAlpha.Y);
	
	float Epsilon = 0.001f;
	if (DistX <= Epsilon)
	{
		return 0;
	}
	else if (DistY <= Epsilon)
	{
		return 1;
	}
	else
	{
		return 2;
	}
}

void UMorphingMeshComponent::SetType(EMorphTargetType NewType)
{
	if (MeshType == NewType)
	{
		return;
	}
	
	if (MorphingMeshData == nullptr)
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

	if (bIsSkeletalMesh)
	{
		BaseSkeletalMeshComponent->SetVisibility(false);
	}
	else
	{
		BaseMeshComponent->SetVisibility(false);
	}
	LiquidMeshComponent->bRenderingEnable = true;

	SetComponentTickEnabled(true);
}
