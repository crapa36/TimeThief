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
#include "Components/TimelineComponent.h"
#include "Network/NetworkGameInstanceSubsystem.h"


// Sets default values
AChestActor::AChestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ChestTimelineComponent = CreateDefaultSubobject<UTimelineComponent>(TEXT("ChestTimelineComponent"));

	LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
	LidMesh->SetupAttachment(MeshComponent);
}

// Called when the game starts or when spawned
void AChestActor::BeginPlay()
{
	Super::BeginPlay();

	if (ChestTimelineComponent && OpenCurve)
	{
		FOnTimelineFloat TimelineProgress;
		TimelineProgress.BindUFunction(this, FName("HandleTimelineProgress"));
		ChestTimelineComponent->AddInterpFloat(OpenCurve, TimelineProgress);
	}

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

	if (ChestTimelineComponent && OpenCurve)
	{
		ChestTimelineComponent->PlayFromStart();
	}
}

void AChestActor::ResetToClosedPose()
{
	if (ChestTimelineComponent && OpenCurve)
	{
		ChestTimelineComponent->Stop();

		ChestTimelineComponent->SetPlaybackPosition(0.0f, false);
	}

	HandleTimelineProgress(0.0f);
}

void AChestActor::UpdateInteractionWidgetLocation()
{
	if (!InteractionWidgetComponent || !MeshComponent)
	{
		return;
	}

	const float WidgetHeight = MeshComponent->Bounds.BoxExtent.Z + InteractionWidgetHeightOffset;
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

void AChestActor::HandleTimelineProgress(float Value)
{
	if (LidMesh)
	{
		FRotator NewLocation = FMath::Lerp(StartRotation, TargetRotation, Value);
		LidMesh->SetRelativeRotation(NewLocation);
	}
}
