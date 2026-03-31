#include "Character/TimeThiefPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimeThiefHeroComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Character/TimeThiefPawnData.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Components/System/InventorySystemComponent.h"
#include "Components/TimeThiefTrajectoryComponent.h"
#include "Actors/InteractionActorBase.h"
#include "Character/TimeThiefPlayerController.h"
#include "ChannelCommons.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "TimeThiefPlayerState.h"
#include "Components/System/TimePointSystemComponent.h"
#include "UI/TimeThiefHUDWidget.h"

ATimeThiefPlayerCharacter::ATimeThiefPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	HeroComponent = CreateDefaultSubobject<UTimeThiefHeroComponent>(TEXT("HeroComponent"));
	PlayerCombatComponent = CreateDefaultSubobject<UTimeThiefPlayerCombatComponent>(TEXT("PlayerCombatComponent"));
	WireComponent = CreateDefaultSubobject<UTimeThiefWireComponent>(TEXT("WireComponent"));
	InventoryComponent = CreateDefaultSubobject<UInventorySystemComponent>(TEXT("InventoryComponent"));

	CharacterTrajectoryComponent = CreateDefaultSubobject<UTimeThiefTrajectoryComponent>(TEXT("CharacterTrajectoryComponent"));
	CharacterTrajectoryComponent->SetAutoActivate(true);
	CharacterTrajectoryComponent->PrimaryComponentTick.bCanEverTick = true;
	CharacterTrajectoryComponent->PrimaryComponentTick.bStartWithTickEnabled = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->bUseControllerDesiredRotation = false;

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;

	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCastShadow(false);
}

bool ATimeThiefPlayerCharacter::PurchaseItem(const FStoreOrder& Order)
{
	if (TimePointSystemComponent->GetTimePoints() >= Order.Price)
	{
		TimePointSystemComponent->ModifyTimePoints(-Order.Price);
		ATimeThiefPlayerState* PS = Cast<ATimeThiefPlayerState>(GetPlayerState());
		switch (Order.ItemID)
		{
		case EItemID::DamageUpgrade:
			PS->Status.Damage++;
			break;
		case EItemID::StabilityUpgrade:
			PS->Status.Stability++;
			break;
		case EItemID::CapacityUpgrade:
			PS->Status.Capacity++;
			break;
		case EItemID::HealthUpgrade:
			PS->Status.Health++;
			HealthComponent->Upgrade();
			break;
		case EItemID::SpeedUpgrade:
			PS->Status.Speed++;
			break;
		default:
			InventoryComponent->AddItem(Order.ItemID);
			break;
		}
		return true;
	}
	
	return false;
}

void ATimeThiefPlayerCharacter::SetPawnData(const UTimeThiefPawnData* InPawnData)
{
	check(InPawnData);

	if (PawnData)
	{
		return;
	}

	PawnData = InPawnData;

	if (HeroComponent)
	{
		HeroComponent->SetPawnData(InPawnData);
		
		if (InputComponent && !HeroComponent->IsReadyToBindInputs())
		{
			HeroComponent->InitializePlayerInput(InputComponent);
		}
	}

	OnPawnDataSet();
}

void ATimeThiefPlayerCharacter::OnPawnDataSet()
{
	if (PawnData && PawnData->PawnTags.Num() > 0)
	{
		AppendOwnedGameplayTags(PawnData->PawnTags);
	}

	if (ATimeThiefPlayerController* PC = Cast<ATimeThiefPlayerController>(GetController()))
	{
		if (UTimeThiefHUDWidget* HUD = PC->GetHUDWidget())
		{
			HUD->InitializeHUD(this);
		}
	}
}

UTimeThiefPawnCombatComponent* ATimeThiefPlayerCharacter::GetCombatComponent() const
{
	return PlayerCombatComponent;
}

USkeletalMeshComponent* ATimeThiefPlayerCharacter::GetWeaponAttachMesh() const
{
	return FirstPersonMesh;
}

USkeletalMeshComponent* ATimeThiefPlayerCharacter::GetMontagePlaybackMesh() const
{
	return FirstPersonMesh;
}

void ATimeThiefPlayerCharacter::ApplyPerspective()
{
	Super::ApplyPerspective();

	if (IsLocallyControlled() && bIsFirstPerson)
	{
		FollowCamera->SetActive(false);
	}
	
}

void ATimeThiefPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!PawnData && DefaultPawnData)
	{
		SetPawnData(DefaultPawnData);
	}

	if (UTimeThiefHealthComponent* Health = GetHealthComponent())
	{
		Health->OnDeath.AddDynamic(this, &ATimeThiefPlayerCharacter::OnDeath);
	}

	GetWorldTimerManager().SetTimer(
		InteractCheckTimerHandle,
		this,
		&ATimeThiefPlayerCharacter::CheckInteractableObject,
		0.1f,
		true
	);
	
}

void ATimeThiefPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ATimeThiefPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (UTimeThiefPawnExtensionComponent* PawnExtComp = FindComponentByClass<UTimeThiefPawnExtensionComponent>())
	{
		PawnExtComp->CheckDefaultInitialization();
	}
}

void ATimeThiefPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (UTimeThiefPawnExtensionComponent* PawnExtComp = FindComponentByClass<UTimeThiefPawnExtensionComponent>())
	{
		PawnExtComp->NotifyControllerChanged();
	}

	if (PawnData)
	{
		OnPawnDataSet();
	}
}

void ATimeThiefPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	if (UTimeThiefPawnExtensionComponent* PawnExtComp = FindComponentByClass<UTimeThiefPawnExtensionComponent>())
	{
		PawnExtComp->NotifyControllerChanged();
	}
}

void ATimeThiefPlayerCharacter::OnDeath(AActor* OwningActor)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
	GetCharacterMovement()->DisableMovement();
}

void ATimeThiefPlayerCharacter::OnInteract()
{
	if (CurrentLookingActor.IsValid())
	{
		CurrentLookingActor->Interact(this);
	}
}

void ATimeThiefPlayerCharacter::CheckInteractableObject()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	
	FVector StartLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(StartLocation, ViewRotation);
	
	FVector EndLocation = StartLocation + ViewRotation.Vector() * (CameraBoom->TargetArmLength + LookingDistance);
	
	FHitResult Hit;
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		StartLocation,
		EndLocation,
		ECC_InteractTrace
	);
	
	if (bHit && Hit.GetActor())
	{
		if (AInteractionActorBase* InteractionActor = Cast<AInteractionActorBase>(Hit.GetActor()))
		{
			if (CurrentLookingActor != InteractionActor)
			{
				if (CurrentLookingActor.IsValid())
				{
					CurrentLookingActor->SetVisibilityInteractionUI(false);
				}
				
				CurrentLookingActor = InteractionActor;
				CurrentLookingActor->SetVisibilityInteractionUI(true);
			}
			return;
		}
	}
	
	if (CurrentLookingActor.IsValid())
	{
		CurrentLookingActor->SetVisibilityInteractionUI(false);
		CurrentLookingActor.Reset();
	}
}

void ATimeThiefPlayerCharacter::AddVicinityItem(AItemBase* Item)
{
	if (const int Index = VicinityItem.AddUnique(Item); Index != INDEX_NONE)
	{
		OnVicinityItemUpdatedEvent.Broadcast();
	}
}

void ATimeThiefPlayerCharacter::RemoveVicinityItem(AItemBase* Item)
{
	if (const int Removed = VicinityItem.Remove(Item); Removed != 0)
	{
		OnVicinityItemUpdatedEvent.Broadcast();
	}
}

void ATimeThiefPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PawnData && DefaultPawnData)
	{
		SetPawnData(DefaultPawnData);
	}

	if (HeroComponent)
	{
		HeroComponent->InitializePlayerInput(PlayerInputComponent);
	}
}