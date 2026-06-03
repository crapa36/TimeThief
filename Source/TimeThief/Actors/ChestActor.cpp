


#include "ChestActor.h"

#include "Animation/AnimSequenceBase.h"
#include "ChannelCommons.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Network/NetworkGameInstanceSubsystem.h"


// Sets default values
AChestActor::AChestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(RootComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshComponent->SetCollisionResponseToChannel(ECC_InteractTrace, ECR_Block);
	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
}

// Called when the game starts or when spawned
void AChestActor::BeginPlay()
{
	Super::BeginPlay();

	ConfigureVisualMode();
	ResetToClosedPose();
	UpdateInteractionWidgetLocation();
}

void AChestActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ConfigureVisualMode();
	UpdateInteractionWidgetLocation();
}

// Called every frame
void AChestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AChestActor::Interact(const ATimeThiefPlayerCharacter* Player)
{
	if (bIsOpened)
	{
		return;
	}

	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		uint32 ChestEntityId = GetEntityId();
		NGIS->SendChestInteract(ChestEntityId);
	}
}

void AChestActor::OpenChest()
{
	if (bIsOpened)
	{
		return;
	}

	bIsOpened = true;
	SetVisibilityInteractionUI(false);
	PlayRewardBurstFX();

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetCollisionResponseToChannel(ECC_InteractTrace, ECR_Ignore);
	}

	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkeletalMeshComponent->SetCollisionResponseToChannel(ECC_InteractTrace, ECR_Ignore);
	}

	if (!IsSkeletalChestMode())
	{
		return;
	}

	if (!OpenAnimation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Chest] OpenChest skipped animation. Actor=%s, HasAnimation=false"),
			*GetNameSafe(this));
		return;
	}

	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkeletalMeshComponent->bPauseAnims = false;
	SkeletalMeshComponent->bNoSkeletonUpdate = false;
	SkeletalMeshComponent->PlayAnimation(OpenAnimation, false);
}

bool AChestActor::IsSkeletalChestMode() const
{
	return bUseSkeletalChest && SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMeshAsset() != nullptr;
}

void AChestActor::ConfigureVisualMode()
{
	const bool bUseSkeletalMode = IsSkeletalChestMode();

	if (MeshComponent)
	{
		MeshComponent->SetVisibility(!bUseSkeletalMode, false);
		MeshComponent->SetHiddenInGame(bUseSkeletalMode, false);
		MeshComponent->SetCollisionEnabled(bUseSkeletalMode ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
		MeshComponent->SetCollisionResponseToChannel(ECC_InteractTrace, bUseSkeletalMode ? ECR_Ignore : ECR_Block);
	}

	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->SetVisibility(bUseSkeletalMode, true);
		SkeletalMeshComponent->SetHiddenInGame(!bUseSkeletalMode);
		SkeletalMeshComponent->SetCollisionEnabled(bUseSkeletalMode ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		SkeletalMeshComponent->SetCollisionResponseToChannel(ECC_InteractTrace, bUseSkeletalMode ? ECR_Block : ECR_Ignore);
	}

	SetVisibilityInteractionUI(false);
}

void AChestActor::ResetToClosedPose()
{
	if (!IsSkeletalChestMode() || !OpenAnimation)
	{
		return;
	}

	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkeletalMeshComponent->bPauseAnims = false;
	SkeletalMeshComponent->bNoSkeletonUpdate = false;
	SkeletalMeshComponent->PlayAnimation(OpenAnimation, false);
	SkeletalMeshComponent->SetPosition(0.0f, false);
	SkeletalMeshComponent->bPauseAnims = true;
	SkeletalMeshComponent->RefreshBoneTransforms();
}

void AChestActor::UpdateInteractionWidgetLocation()
{
	if (!InteractionWidgetComponent)
	{
		return;
	}

	const float MeshHalfHeight = IsSkeletalChestMode() && SkeletalMeshComponent
		? SkeletalMeshComponent->Bounds.BoxExtent.Z
		: MeshComponent ? MeshComponent->Bounds.BoxExtent.Z : 0.0f;
	const float WidgetHeight = MeshHalfHeight + InteractionWidgetHeightOffset;
	InteractionWidgetComponent->SetRelativeLocation(FVector{0.0f, 0.0f, WidgetHeight});
}

void AChestActor::PlayRewardBurstFX()
{
	if (bRewardBurstFXPlayed || !RewardBurstFX)
	{
		return;
	}

	bRewardBurstFXPlayed = true;

	const FVector SpawnLocation = GetActorLocation() + GetActorTransform().TransformVectorNoScale(RewardBurstFXOffset);
	UNiagaraComponent* SpawnedFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		RewardBurstFX,
		SpawnLocation,
		GetActorRotation()
	);

	if (SpawnedFX)
	{
		SpawnedFX->SetWorldScale3D(RewardBurstFXScale);
	}
}

