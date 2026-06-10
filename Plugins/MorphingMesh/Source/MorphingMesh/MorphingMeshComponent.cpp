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

	if (ElapsedTime >= MaxMorphingTime)
	{
		CurrAlpha = DestAlpha;

		if (!bDebug)
		{
			if (bIsSkeletalMesh)
			{
				BaseSkeletalMeshComponent->SetVisibility(true);
				BaseSkeletalMeshComponent->CastShadow = true;
			}
			else
			{
				BaseMeshComponent->SetVisibility(true);
			}
			LiquidMeshComponent->bRenderingEnable = false;
		}

		OnMorphTargetTypeChangedSignature.Broadcast(MeshType);
		SetComponentTickEnabled(false);
	}
	else
	{
		ElapsedTime += DeltaTime;
		const float AlphaRatio = FMath::Min(ElapsedTime / MaxMorphingTime, 1.0f);
		CurrAlpha = FMath::Lerp(PrevAlpha, DestAlpha, AlphaRatio);
		if (bIsSkeletalMesh)
		{
			int Index = GetActiveSkeletalIndex();

			if (Index != -1)
			{
				if (Index != PrevIndex)
				{
					BaseSkeletalMeshComponent->EmptyOverrideMaterials();
					BaseSkeletalMeshComponent->SetSkeletalMesh(MorphingMeshData->SkeletalMeshes[Index], bIsDifferentSkeletal);
					PrevIndex = Index;
				}
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

void UMorphingMeshComponent::SetSkeletalMeshComponent(USkeletalMeshComponent* NewSkeletalMeshComponent)
{
	BaseSkeletalMeshComponent = NewSkeletalMeshComponent;
	Check();
}

void UMorphingMeshComponent::SetBoneMatrixSourceSkeletalMeshComponent(USkeletalMeshComponent* NewSkeletalMeshComponent)
{
	BoneMatrixSourceSkeletalMeshComponent = NewSkeletalMeshComponent;
}

USkeletalMeshComponent* UMorphingMeshComponent::GetBoneMatrixSourceSkeletalMeshComponent() const
{
	return BoneMatrixSourceSkeletalMeshComponent ? BoneMatrixSourceSkeletalMeshComponent : BaseSkeletalMeshComponent;
}

int UMorphingMeshComponent::GetActiveSkeletalIndex() const
{
	if (CurrAlpha == FVector::ZeroVector)
	{
		return -1;
	}

	if (!bIsSkeletalMesh)
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
	DestAlpha = FVector::ZeroVector;

	if (NewType == EMorphTargetType::None)
	{
		ElapsedTime = MaxMorphingTime;
		BaseMeshComponent->SetStaticMesh(nullptr);
	}
	else
	{
		DestAlpha[Index] = 1.0f;

		ElapsedTime = MaxMorphingTime * CurrAlpha[Index];
		BaseMeshComponent->SetStaticMesh(MorphingMeshData->GetBaseMeshes()[Index]);
	}

	MeshType = NewType;
	PrevAlpha = CurrAlpha;

	if (bIsSkeletalMesh)
	{
		BaseSkeletalMeshComponent->SetVisibility(false);
		BaseSkeletalMeshComponent->CastShadow = false;
	}
	else
	{
		BaseMeshComponent->SetVisibility(false);
	}
	LiquidMeshComponent->bRenderingEnable = true;

	SetComponentTickEnabled(true);
}

void UMorphingMeshComponent::Check()
{
	if (MorphingMeshData == nullptr)
	{
		bIsSkeletalMesh = false;
		bIsValid = false;
		return;
	}

	if (MorphingMeshData->Material)
	{
		LiquidMaterial = MorphingMeshData->Material;
	}

	bIsSkeletalMesh = MorphingMeshData->IsSkeletalValid() && BaseSkeletalMeshComponent;
	bIsValid = MorphingMeshData->IsValid();

	if (bDebug)
	{
		LiquidMeshComponent->bRenderingEnable = true;
		if (BaseSkeletalMeshComponent)
		{
			BaseSkeletalMeshComponent->SetVisibility(false);
		}
		BaseMeshComponent->SetVisibility(false);
	}
	else
	{
		LiquidMeshComponent->bRenderingEnable = false;
	}

	int Index = int(MeshType);
	PrevIndex = Index;
	PrevAlpha = FVector::ZeroVector;
	if (MeshType != EMorphTargetType::None)
	{
		PrevAlpha[Index] = 1.0f;
		if (bIsSkeletalMesh)
		{
			BaseSkeletalMeshComponent->EmptyOverrideMaterials();
			BaseSkeletalMeshComponent->SetSkeletalMesh(MorphingMeshData->SkeletalMeshes[Index]);
			BaseSkeletalMeshComponent->VisibilityBasedAnimTickOption =
				EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			BaseMeshComponent->SetVisibility(false);
			
			bIsDifferentSkeletal = false;
			auto Skeletal = MorphingMeshData->SkeletalMeshes[Index]->Skeleton;
			for (auto SkeletalMesh : MorphingMeshData->SkeletalMeshes)
			{
				if (Skeletal != SkeletalMesh->Skeleton)
				{
					bIsDifferentSkeletal = true;
					break;
				}
			}
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
