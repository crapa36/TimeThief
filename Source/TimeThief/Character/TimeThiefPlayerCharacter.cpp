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
#include "TimeThiefPlayerState.h"
#include "Components/System/TimePointSystemComponent.h"
#include "UI/TimeThiefHUDWidget.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Network/NetworkWireComponent.h"
#include "Game/ItemSettings.h"
#include "NiagaraFunctionLibrary.h"
#include "Animation/TimeThiefAnimInstance.h"
#include "TimeThiefGameplayTags.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"

ATimeThiefPlayerCharacter::ATimeThiefPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	CameraBoom->CameraLagSpeed = DefaultCameraLagSpeed;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	HeroComponent = CreateDefaultSubobject<UTimeThiefHeroComponent>(TEXT("HeroComponent"));
	PlayerCombatComponent = CreateDefaultSubobject<UTimeThiefPlayerCombatComponent>(TEXT("PlayerCombatComponent"));
	WireComponent = CreateDefaultSubobject<UTimeThiefWireComponent>(TEXT("WireComponent"));
	NetworkWireComponent = CreateDefaultSubobject<UNetworkWireComponent>(TEXT("NetworkWireComponent"));
	InventoryComponent = CreateDefaultSubobject<UInventorySystemComponent>(TEXT("InventoryComponent"));

	CharacterTrajectoryComponent = CreateDefaultSubobject<UTimeThiefTrajectoryComponent>(
		TEXT("CharacterTrajectoryComponent"));
	CharacterTrajectoryComponent->SetAutoActivate(true);
	CharacterTrajectoryComponent->PrimaryComponentTick.bCanEverTick = true;
	CharacterTrajectoryComponent->PrimaryComponentTick.bStartWithTickEnabled = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
	GetCharacterMovement()->JumpZVelocity = BaseJumpVelocity;
	JumpMaxCount = 2;

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;

	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCastShadow(false);
}

bool ATimeThiefPlayerCharacter::PurchaseItem(const FStoreOrder& Order)
{
	if (!TimePointSystemComponent)
	{
		return false;
	}

	if (TimePointSystemComponent->GetTimePoints() < Order.Price)
	{
		return false;
	}

	ATimeThiefPlayerState* PS = GetPlayerState<ATimeThiefPlayerState>();

	switch (Order.ItemID)
	{
	case EItemID::DamageUpgrade:
	case EItemID::StabilityUpgrade:
	case EItemID::CapacityUpgrade:
	case EItemID::SpeedUpgrade:
		if (!PS)
		{
			return false;
		}
		break;
	case EItemID::HealthUpgrade:
		if (!PS || !HealthComponent)
		{
			return false;
		}
		break;
	default:
		if (!InventoryComponent)
		{
			return false;
		}
		break;
	}

	if (!TimePointSystemComponent->ModifyTimePoints(-Order.Price))
	{
		return false;
	}

	UTimeThiefWeaponComponentBase* CurrentWeaponComp = nullptr;
	if (PlayerCombatComponent)
	{
		if (ATimeThiefMasterWeapon* MasterWeapon = PlayerCombatComponent->GetMasterWeapon())
		{
			CurrentWeaponComp = MasterWeapon->GetActiveWeaponComponent();
		}
	}

	const UUpgradeData* UpgradeBalanceData = nullptr;
	if (const UItemSettings* ItemSettings = GetDefault<UItemSettings>())
	{
		UpgradeBalanceData = ItemSettings->UpgradeData.LoadSynchronous();
	}

	TMap<FGameplayTag, FUpgradeFloatLevels> DamageTable = UpgradeBalanceData
		                                                      ? UpgradeBalanceData->DamageBonusByWeaponAndLevel
		                                                      : DamageBonusByWeaponAndLevel;
	TMap<FGameplayTag, FUpgradeIntLevels> CapacityTable = UpgradeBalanceData
		                                                      ? UpgradeBalanceData->CapacityBonusByWeaponAndLevel
		                                                      : CapacityBonusByWeaponAndLevel;
	TMap<FGameplayTag, FUpgradeFloatLevels> RecoilTable = UpgradeBalanceData
		                                                      ? UpgradeBalanceData->RecoilReductionByWeaponAndLevel
		                                                      : RecoilReductionByWeaponAndLevel;
	TArray<float> MoveSpeedTable = UpgradeBalanceData
		                               ? UpgradeBalanceData->MoveSpeedBonusPerLevel
		                               : MoveSpeedBonusPerLevel;
	TArray<float> JumpVelocityTable = UpgradeBalanceData
		                                  ? UpgradeBalanceData->JumpVelocityBonusPerLevel
		                                  : JumpVelocityBonusPerLevel;

	const FTimeThiefGameplayTags& GameplayTags = FTimeThiefGameplayTags::Get();

	auto EnsureFloatLevels = [](TMap<FGameplayTag, FUpgradeFloatLevels>& Table, const FGameplayTag& Tag,
	                            std::initializer_list<float> Defaults)
	{
		if (!Tag.IsValid())
		{
			return;
		}

		FUpgradeFloatLevels* Found = Table.Find(Tag);
		if (!Found || Found->Values.Num() == 0)
		{
			FUpgradeFloatLevels Levels;
			Levels.Values.Append(Defaults);
			Table.Add(Tag, Levels);
		}
	};

	auto EnsureIntLevels = [](TMap<FGameplayTag, FUpgradeIntLevels>& Table, const FGameplayTag& Tag,
	                          std::initializer_list<int32> Defaults)
	{
		if (!Tag.IsValid())
		{
			return;
		}

		FUpgradeIntLevels* Found = Table.Find(Tag);
		if (!Found || Found->Values.Num() == 0)
		{
			FUpgradeIntLevels Levels;
			Levels.Values.Append(Defaults);
			Table.Add(Tag, Levels);
		}
	};

	if (MoveSpeedTable.Num() == 0)
	{
		MoveSpeedTable = {50.0f, 50.0f, 50.0f, 50.0f, 50.0f};
	}

	if (JumpVelocityTable.Num() == 0)
	{
		JumpVelocityTable = {30.0f, 30.0f, 30.0f, 30.0f, 30.0f};
	}

	EnsureFloatLevels(DamageTable, GameplayTags.Weapon_Rifle, {2.0f, 2.0f, 3.0f, 3.0f, 4.0f});
	EnsureFloatLevels(DamageTable, GameplayTags.Weapon_Shotgun, {0.5f, 0.5f, 1.0f, 1.0f, 1.5f});
	EnsureFloatLevels(DamageTable, GameplayTags.Weapon_RocketLauncher, {25.0f, 25.0f, 30.0f, 30.0f, 40.0f});

	EnsureIntLevels(CapacityTable, GameplayTags.Weapon_Rifle, {5, 5, 5, 5, 5});
	EnsureIntLevels(CapacityTable, GameplayTags.Weapon_Shotgun, {1, 1, 1, 1, 1});
	EnsureIntLevels(CapacityTable, GameplayTags.Weapon_RocketLauncher, {0, 0, 1, 0, 1});

	EnsureFloatLevels(RecoilTable, GameplayTags.Weapon_Rifle, {0.05f, 0.05f, 0.05f, 0.05f, 0.05f});
	EnsureFloatLevels(RecoilTable, GameplayTags.Weapon_Shotgun, {0.2f, 0.2f, 0.2f, 0.2f, 0.2f});
	EnsureFloatLevels(RecoilTable, GameplayTags.Weapon_RocketLauncher, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

	auto RecalculateUpgradeStats = [PS, &DamageTable, &CapacityTable, &RecoilTable, &MoveSpeedTable, &JumpVelocityTable
		]()
	{
		PS->RecalculateAppliedUpgradeStats(
			DamageTable,
			CapacityTable,
			RecoilTable,
			MoveSpeedTable,
			JumpVelocityTable
		);
	};

	auto ApplyCurrentWeaponUpgradeStats = [PS, CurrentWeaponComp]()
	{
		if (!PS || !CurrentWeaponComp)
		{
			return;
		}

		if (const FAppliedWeaponUpgradeStats* WeaponStats = PS->GetAppliedWeaponUpgradeStats(
			CurrentWeaponComp->GetWeaponTag()))
		{
			CurrentWeaponComp->SetDamageBonus(WeaponStats->DamageBonus);
			CurrentWeaponComp->SetCapacityBonus(WeaponStats->CapacityBonusAmmo);
			CurrentWeaponComp->SetRecoilReduction(WeaponStats->RecoilReduction);
			return;
		}

		CurrentWeaponComp->SetDamageBonus(0.0f);
		CurrentWeaponComp->SetCapacityBonus(0);
		CurrentWeaponComp->SetRecoilReduction(0.0f);
	};

	switch (Order.ItemID)
	{
	case EItemID::DamageUpgrade:
		PS->Status.Damage++;
		RecalculateUpgradeStats();
		ApplyCurrentWeaponUpgradeStats();
		break;
	case EItemID::StabilityUpgrade:
		PS->Status.Stability++;
		RecalculateUpgradeStats();
		ApplyCurrentWeaponUpgradeStats();
		break;
	case EItemID::CapacityUpgrade:
		PS->Status.Capacity++;
		RecalculateUpgradeStats();
		ApplyCurrentWeaponUpgradeStats();
		break;
	case EItemID::HealthUpgrade:
		PS->Status.Health++;
		HealthComponent->Upgrade();
		break;
	case EItemID::SpeedUpgrade:
		PS->Status.Speed++;
		RecalculateUpgradeStats();

#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("[Store][SpeedUpgrade] Level=%d MoveSpeedBonus=%.2f JumpBonus=%.2f"),
		       PS->Status.Speed,
		       PS->AppliedUpgradeStats.MoveSpeedBonus,
		       PS->AppliedUpgradeStats.JumpVelocityBonus);
#endif

		if (PlayerCombatComponent)
		{
			PlayerCombatComponent->SetMoveSpeedUpgradeBonus(PS->AppliedUpgradeStats.MoveSpeedBonus);
		}
		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			MovementComp->JumpZVelocity = BaseJumpVelocity + PS->AppliedUpgradeStats.JumpVelocityBonus;

#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Log,
			       TEXT("[Store][SpeedUpgrade] FinalMoveSpeed(CombatBaseApplied) Base=%.2f Bonus=%.2f JumpFinal=%.2f"),
			       BaseMoveSpeed,
			       PS->AppliedUpgradeStats.MoveSpeedBonus,
			       MovementComp->JumpZVelocity);
#endif
		}
		break;
	default:
		InventoryComponent->AddItem(Order.ItemID);
		break;
	}

	return true;
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
	if (IsLocallyControlled() && bIsFirstPerson && FirstPersonMesh)
	{
		return FirstPersonMesh;
	}

	return GetThirdPersonMesh();
}

USkeletalMeshComponent* ATimeThiefPlayerCharacter::GetMontagePlaybackMesh() const
{
	if (IsLocallyControlled() && bIsFirstPerson && FirstPersonMesh)
	{
		return FirstPersonMesh;
	}

	return GetThirdPersonMesh();
}

void ATimeThiefPlayerCharacter::ApplyPerspective()
{
	Super::ApplyPerspective();

	if (IsLocallyControlled() && FollowCamera)
	{
		FollowCamera->SetActive(!bIsFirstPerson);
	}
}

void ATimeThiefPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	CameraBoom->bEnableCameraLag = true;
	CachedCameraLagSpeed = CameraBoom->CameraLagSpeed;

	if (!PawnData && DefaultPawnData)
	{
		SetPawnData(DefaultPawnData);
	}

	if (UTimeThiefHealthComponent* Health = GetHealthComponent())
	{
		Health->OnDeath.AddDynamic(this, &ATimeThiefPlayerCharacter::OnDeath);
	}

	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->MaxWalkSpeed = BaseMoveSpeed;
		MovementComp->JumpZVelocity = BaseJumpVelocity;
	}

	GetWorldTimerManager().SetTimer(
		InteractCheckTimerHandle,
		this,
		&ATimeThiefPlayerCharacter::CheckInteractableObject,
		0.1f,
		true
	);

	if (WireComponent)
	{
		WireComponent->OnWireStateChanged.AddDynamic(this, &ATimeThiefPlayerCharacter::OnWireStateChanged);
	}

	GetWorldTimerManager().SetTimer(
		InteractCheckTimerHandle,
		this,
		&ATimeThiefPlayerCharacter::CheckInteractableObject,
		0.1f,
		true
	);

	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->MaxWalkSpeed = BaseMoveSpeed;
		MovementComp->JumpZVelocity = BaseJumpVelocity;
	}
}

void ATimeThiefPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ATimeThiefPlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	SendJumpEventToServer();
	if (JumpMaxCount < 2 || JumpCurrentCount != JumpMaxCount)
	{
		return;
	}

	if (DoubleJumpEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			DoubleJumpEffect,
			GetThirdPersonMesh()->GetComponentLocation(),
			GetActorRotation()
		);
	}

	if (UTimeThiefAnimInstance* AnimInst = Cast<UTimeThiefAnimInstance>(GetThirdPersonMesh()->GetAnimInstance()))
	{
		AnimInst->TriggerDoubleJump();
	}

	if (FirstPersonMesh)
	{
		if (UTimeThiefAnimInstance* FPAnimInst = Cast<UTimeThiefAnimInstance>(FirstPersonMesh->GetAnimInstance()))
		{
			FPAnimInst->TriggerDoubleJump();
		}
	}
}

void ATimeThiefPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	SendJumpLandEventToServer();
}

void ATimeThiefPlayerCharacter::SendJumpEventToServer()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->SendJump();
	}
}

void ATimeThiefPlayerCharacter::SendJumpLandEventToServer()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		NGIS->SendJumpLand();
	}
}

void ATimeThiefPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (HeroComponent)
	{
		HeroComponent->CheckDefaultInitialization();
	}
}

void ATimeThiefPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (HeroComponent)
	{
		HeroComponent->RebuildCachedComponents();
	}

	if (PawnData)
	{
		OnPawnDataSet();
	}
}

void ATimeThiefPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HeroComponent)
	{
		HeroComponent->NotifyControllerChanged();
		HeroComponent->RebuildCachedComponents();
	}
}

void ATimeThiefPlayerCharacter::OnDeath()
{
	Super::OnDeath();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
}

void ATimeThiefPlayerCharacter::OnBeginRespawn()
{
	Super::OnBeginRespawn();
}

void ATimeThiefPlayerCharacter::OnEndRespawn()
{
	Super::OnEndRespawn();

	if (HeroComponent)
	{
		HeroComponent->RebuildCachedComponents();
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}
}

void ATimeThiefPlayerCharacter::OnWireStateChanged(EWireState OldState, EWireState NewState)
{
	if (NewState == EWireState::Attached)
	{
		CameraBoom->bEnableCameraLag = true;
		CameraBoom->CameraLagSpeed = WireCameraLagSpeed;
		return;
	}

	if (OldState == EWireState::Attached)
	{
		CameraBoom->bEnableCameraLag = true;
		CameraBoom->CameraLagSpeed = CachedCameraLagSpeed;
	}
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
