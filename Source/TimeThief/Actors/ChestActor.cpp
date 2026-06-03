


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

	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetHiddenInGame(true);
	}

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(RootComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SkeletalMeshComponent->SetCollisionResponseToChannel(ECC_InteractTrace, ECR_Block);
	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
}

// Called when the game starts or when spawned
void AChestActor::BeginPlay()
{
	Super::BeginPlay();

	ResetToClosedPose();
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

	if (!SkeletalMeshComponent || !OpenAnimation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Chest] OpenChest skipped animation. Actor=%s, HasMesh=%s, HasAnimation=%s"),
			*GetNameSafe(this),
			SkeletalMeshComponent ? TEXT("true") : TEXT("false"),
			OpenAnimation ? TEXT("true") : TEXT("false"));
		return;
	}

	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkeletalMeshComponent->SetCollisionResponseToChannel(ECC_InteractTrace, ECR_Ignore);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SkeletalMeshComponent->bPauseAnims = false;
	SkeletalMeshComponent->bNoSkeletonUpdate = false;
	SkeletalMeshComponent->PlayAnimation(OpenAnimation, false);
}

void AChestActor::ResetToClosedPose()
{
	if (!SkeletalMeshComponent || !OpenAnimation)
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
	if (!InteractionWidgetComponent || !SkeletalMeshComponent)
	{
		return;
	}

	const float WidgetHeight = SkeletalMeshComponent->Bounds.BoxExtent.Z + InteractionWidgetHeightOffset;
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

