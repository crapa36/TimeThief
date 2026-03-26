#include "Character/TimeThiefPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimeThiefHeroComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Character/TimeThiefPawnData.h"
#include "CharacterTrajectoryComponent.h"
#include "Actors/InteractionActorBase.h"
#include "Components/System/InventorySystemComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "ChannelCommons.h"

ATimeThiefPlayerCharacter::ATimeThiefPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	
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

	CharacterTrajectoryComponent = CreateDefaultSubobject<UCharacterTrajectoryComponent>(TEXT("CharacterTrajectoryComponent"));
	CharacterTrajectoryComponent->SetAutoActivate(true);
	CharacterTrajectoryComponent->PrimaryComponentTick.bCanEverTick = true;
	CharacterTrajectoryComponent->PrimaryComponentTick.bStartWithTickEnabled = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
}

void ATimeThiefPlayerCharacter::OnInteract()
{
	if (CurrentLookingActor.IsValid())
	{
		CurrentLookingActor->Interact(this);
	}
}

void ATimeThiefPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATimeThiefPlayerCharacter, PawnData);
}

void ATimeThiefPlayerCharacter::SetPawnData(const UTimeThiefPawnData* InPawnData) {
	check(InPawnData);

	if (PawnData) {
		return;
	}

	PawnData = InPawnData;
	OnPawnDataSet();
}

void ATimeThiefPlayerCharacter::OnPawnDataSet() {
	if (HeroComponent && PawnData) {
		HeroComponent->SetPawnData(PawnData);

		if (InputComponent) {
			HeroComponent->InitializePlayerInput(InputComponent);
		}
	}

	if (PawnData && PawnData->PawnTags.Num() > 0) {
		AppendOwnedGameplayTags(PawnData->PawnTags);
	}
}

void ATimeThiefPlayerCharacter::OnRep_PawnData() {
	OnPawnDataSet();
}

UTimeThiefPawnCombatComponent* ATimeThiefPlayerCharacter::GetPawnCombatComponent() const {
	return PlayerCombatComponent;
}

void ATimeThiefPlayerCharacter::BeginPlay() {
	Super::BeginPlay();

	if (UTimeThiefHealthComponent* Health = GetHealthComponent()) {
		Health->OnDeath.AddDynamic(this, &ATimeThiefPlayerCharacter::OnDeath);
	}

	if (IsLocallyControlled() && bIsFirstPerson)
	{
		FollowCamera->SetActive(false);
	}
	
	GetWorldTimerManager().SetTimer(
		InteractCheckTimerHandle, 
		this, 
		&ATimeThiefPlayerCharacter::CheckInteractableObject, 
		0.1f, 
		true
		);
}

void ATimeThiefPlayerCharacter::OnDeath(AActor* OwningActor) {
	if (APlayerController* PC = Cast<APlayerController>(GetController())) {
		DisableInput(PC);
	}
	GetCharacterMovement()->DisableMovement();
}

void ATimeThiefPlayerCharacter::CheckInteractableObject()
{
	if (!FollowCamera)
	{
		return;
	}
	
	FVector StartLocation = FollowCamera->GetComponentLocation();
	FVector ForwardVector = FollowCamera->GetForwardVector();
	FVector EndLocation = StartLocation + ForwardVector * (CameraBoom->TargetArmLength + LookingDistance);
	
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

void ATimeThiefPlayerCharacter::AddNearInteractionActor(AInteractionActorBase* InteractionActor)
{
	NearInteractionActors.AddUnique(InteractionActor);
}

void ATimeThiefPlayerCharacter::RemoveNearInteractionActor(AInteractionActorBase* InteractionActor)
{
	NearInteractionActors.Remove(InteractionActor);
}

void ATimeThiefPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (HeroComponent) {
		HeroComponent->InitializePlayerInput(PlayerInputComponent);
	}
}
