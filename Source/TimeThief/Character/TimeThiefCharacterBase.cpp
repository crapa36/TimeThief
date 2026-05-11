#include "Character/TimeThiefCharacterBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/System/TimePointSystemComponent.h"
#include "ItemCommons.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimeThiefPlayerState.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/TimeThiefAnimInstance.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "Components/Skill/SavePointSkillComponent.h"
#include "Components/System/InventorySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MorphingMesh/MorphingMeshData.h"
#include "MorphingMesh/Core/LiquidMeshComponent.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Weapon/TimeThiefMasterWeapon.h"

ATimeThiefCharacterBase::ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HealthComponent = CreateDefaultSubobject<UTimeThiefHealthComponent>(TEXT("HealthComponent"));

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetCapsuleComponent());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCastShadow(false);
	FirstPersonMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	FirstPersonMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	FirstPersonSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("FirstPersonSpringArm"));
	FirstPersonSpringArm->SetupAttachment(GetCapsuleComponent());
	FirstPersonSpringArm->TargetArmLength = 0.0f;
	FirstPersonSpringArm->bUsePawnControlRotation = true;
	FirstPersonSpringArm->SetRelativeLocation(FVector(0.f, 0.f, 160.f));

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(FirstPersonSpringArm);
	FirstPersonCamera->bUsePawnControlRotation = false;
	FirstPersonCamera->SetActive(false);

	bIsFirstPerson = false;

	TimePointSystemComponent = CreateDefaultSubobject<UTimePointSystemComponent>(TEXT("TimePointSystemComponent"));

	DisappearFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DisappearFX"));
	DisappearFX->SetupAttachment(GetMesh());

	DeadFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DeadFX"));
	DeadFX->SetupAttachment(GetMesh());
	DeadFX->bAutoActivate = false;

	SpawnFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SpawnFX"));
	SpawnFX->SetupAttachment(GetMesh());
	SpawnFX->bAutoActivate = false;

	SavePointSkillComponent = CreateDefaultSubobject<USavePointSkillComponent>(TEXT("SavePointSkillComponent"));

	MorphingCharacter = CreateDefaultSubobject<UMorphingMeshComponent>(TEXT("MorphingCharacter"));
	MorphingCharacter->SetupAttachment(GetMesh());
	MorphingCharacter->LiquidMeshComponent->SetupAttachment(MorphingCharacter);
	MorphingCharacter->BaseMeshComponent->SetupAttachment(MorphingCharacter);
	MorphingCharacter->BaseMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	WeaponActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("WeaponActorComponent"));
	WeaponActorComponent->SetupAttachment(GetMesh(), FName{TEXT("HandGrip_R")});
}

void ATimeThiefCharacterBase::OnDeath()
{
	bIsRespawn = false;
	bIsDead = true;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	for (auto Comp : GetComponents())
	{
		if (ILifeObserver* LifeObserver = Cast<ILifeObserver>(Comp))
		{
			LifeObserver->OnDeath();
		}
	}

	DeadFX->Activate(true);
	DisappearFX->SetActive(false, true);
}

void ATimeThiefCharacterBase::OnBeginRespawn()
{
	if (bIsDead)
	{
		bPendingRespawn = false;
		bIsRespawn = true;
		Mask = 0;

		for (auto Comp : GetComponents())
		{
			if (ILifeObserver* LifeObserver = Cast<ILifeObserver>(Comp))
			{
				LifeObserver->OnBeginRespawn();
			}
		}

		DeadFX->Deactivate();
		SpawnFX->Activate(true);
		DisappearFX->SetActive(false, true);
	}
}

void ATimeThiefCharacterBase::OnEndRespawn()
{
	bIsDead = false;
	bIsRespawn = false;

	for (auto Comp : GetComponents())
	{
		if (ILifeObserver* LifeObserver = Cast<ILifeObserver>(Comp))
		{
			LifeObserver->OnEndRespawn();
		}
	}

	SpawnFX->Deactivate();
	DisappearFX->Activate();
}

void ATimeThiefCharacterBase::SetMask(float NewMask)
{
	if (bIsDead)
	{
		return;
	}

	Mask = std::clamp(NewMask, 0.2f, 1.f);

	UpdateMask();
}

void ATimeThiefCharacterBase::AddMask(float Amount)
{
	if (bIsDead)
	{
		return;
	}

	Mask = std::clamp(Mask + Amount, 0.2f, 1.f);

	UpdateMask();
}

USkeletalMeshComponent* ATimeThiefCharacterBase::GetWeaponAttachMesh() const
{
	return GetThirdPersonMesh();
}

USkeletalMeshComponent* ATimeThiefCharacterBase::GetMontagePlaybackMesh() const
{
	return GetThirdPersonMesh();
}

USkeletalMeshComponent* ATimeThiefCharacterBase::GetThirdPersonMesh() const
{
	if (!MorphingCharacter || !MorphingCharacter->bIsSkeletalMesh)
	{
		return GetMesh();
	}

	return MorphingCharacter->BaseSkeletalMeshComponent;
}

ATimeThiefMasterWeapon* ATimeThiefCharacterBase::GetWeaponActor() const
{
	return Cast<ATimeThiefMasterWeapon>(WeaponActorComponent->GetChildActor());
}

void ATimeThiefCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (UTimeThiefHealthComponent* Health = GetHealthComponent())
	{
		Health->OnDeath.AddDynamic(this, &ThisClass::OnDeath);
	}

	DeadFX->SetVariableFloat(FName("User.Loop"), InterpTime);
	SpawnFX->SetVariableFloat(FName("User.MaxLifeTime"), InterpTime);

	OnBeginRespawn();
}

void ATimeThiefCharacterBase::OnPlayerInitialized()
{
	if (IsLocallyControlled())
	{
		if (FirstPersonMesh)
		{
			FirstPersonMesh->SetLeaderPoseComponent(GetMesh());

			FirstPersonMesh->HideBoneByName(FName("head"), EPhysBodyOp::PBO_None);
			FirstPersonMesh->HideBoneByName(FName("neck_01"), EPhysBodyOp::PBO_None);
			FirstPersonMesh->HideBoneByName(FName("neck_02"), EPhysBodyOp::PBO_None);
		}

		ApplyPerspective();
	}
}

void ATimeThiefCharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (IsLocallyControlled())
	{
		OnPlayerInitialized();
	}

	if (UTimeThiefPawnExtensionComponent* Extension = FindComponentByClass<UTimeThiefPawnExtensionComponent>())
	{
		Extension->NotifyControllerChanged();
	}
}

void ATimeThiefCharacterBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	MorphingCharacter->SetSkeletalMeshComponent(GetMesh());
}

void ATimeThiefCharacterBase::TogglePerspective()
{
	bIsFirstPerson = !bIsFirstPerson;

	if (IsLocallyControlled())
	{
		ApplyPerspective();
	}
}

void ATimeThiefCharacterBase::ApplyPerspective()
{
	FirstPersonCamera->SetActive(bIsFirstPerson);
	GetMesh()->SetOwnerNoSee(bIsFirstPerson);

	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(bIsFirstPerson);
	}

	bUseControllerRotationYaw = bIsFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
}

void ATimeThiefCharacterBase::DoubleJump()
{
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

void ATimeThiefCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bPendingRespawn)
	{
		return;
	}

	if (bIsDead && !bIsRespawn)
	{
		Mask = FMath::FInterpConstantTo(Mask, 0, DeltaTime, 1 / InterpTime);
		UpdateMask();

		if (Mask == 0)
		{
			bPendingRespawn = true;
			if (auto GI = GetGameInstance()->GetSubsystem<UNetworkGameInstanceSubsystem>();
				GI && !GI->IsConnected())
			{
				OnBeginRespawn();
			}
		}
	}

	else if (bIsRespawn)
	{
		Mask = FMath::FInterpConstantTo(Mask, 1, DeltaTime, 1 / InterpTime);

		if (Mask == 1.f)
		{
			OnEndRespawn();
		}

		UpdateMask();
	}
}

void ATimeThiefCharacterBase::HandleDeathFromServer()
{
	bIsDead = true;
	bIsRespawn = false;
	bPendingRespawn = false;

	// PlayDeathEffects();
	// NotifyLifeObserversDeath();

	// 아마 아래 코드가 Death 연출 및 필요작업인듯
	DeadFX->Activate(true);
	DisappearFX->SetActive(false, true);

	if (ILifeObserver* PS = Cast<ILifeObserver>(GetPlayerState()))
	{
		PS->OnDeath();
	}
	for (auto Comp : GetComponents())
	{
		if (ILifeObserver* LifeObserver = Cast<ILifeObserver>(Comp))
		{
			LifeObserver->OnDeath();
		}
	}
}

void ATimeThiefCharacterBase::HandleRespawnFromServer(const FVector& RespawnLocation)
{
	SetActorLocation(RespawnLocation, false, nullptr, ETeleportType::TeleportPhysics);

	OnBeginRespawn();
}

void ATimeThiefCharacterBase::FinishRespawnPresentation()
{
	if (!bIsRespawn)
	{
		return;
	}

	bIsRespawn = false;

	// StopRespawnEffects();
	// NotifyLifeObserversEndRespawn();

	SpawnFX->Deactivate();
	DisappearFX->Activate();

	for (auto Comp : GetComponents())
	{
		if (ILifeObserver* LifeObserver = Cast<ILifeObserver>(Comp))
		{
			LifeObserver->OnEndRespawn();
		}
	}
}

void ATimeThiefCharacterBase::UpdateMask()
{
	for (int i = 0; i < GetMesh()->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* MID = GetOrCreateMaterialInstanceDynamic(GetMesh(), i))
		{
			MID->SetScalarParameterValue(FName("Mask"), Mask);
		}
	}

	for (int i = 0; i < GetWeaponActor()->GetWeaponMesh()->GetNumMaterials(); ++i)
	{
		if (UMaterialInstanceDynamic* MID = GetOrCreateMaterialInstanceDynamic(GetWeaponActor()->GetWeaponMesh(), i))
		{
			MID->SetScalarParameterValue(FName("Mask"), Mask);
		}
	}

	DisappearFX->SetVariableFloat(FName("User.Mask"), Mask);
}

void ATimeThiefCharacterBase::PlayMontageOnAllMeshes(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		return;
	}

	if (UAnimInstance* ThirdPersonAnim = GetThirdPersonMesh()->GetAnimInstance())
	{
		ThirdPersonAnim->Montage_Play(Montage, PlayRate);
	}

	if (FirstPersonMesh)
	{
		if (UAnimInstance* FirstPersonAnim = FirstPersonMesh->GetAnimInstance())
		{
			FirstPersonAnim->Montage_Play(Montage, PlayRate);
		}
	}
}

void ATimeThiefCharacterBase::PlayAnimationOnAllMeshes(UAnimSequenceBase* Animation, FName SlotName, float BlendInTime,
                                                       float BlendOutTime, float PlayRate)
{
	if (!Animation)
	{
		return;
	}

	if (UAnimInstance* ThirdPersonAnim = GetThirdPersonMesh()->GetAnimInstance())
	{
		ThirdPersonAnim->PlaySlotAnimationAsDynamicMontage(Animation, SlotName, BlendInTime, BlendOutTime, PlayRate);
	}

	if (FirstPersonMesh)
	{
		if (UAnimInstance* FirstPersonAnim = FirstPersonMesh->GetAnimInstance())
		{
			FirstPersonAnim->
				PlaySlotAnimationAsDynamicMontage(Animation, SlotName, BlendInTime, BlendOutTime, PlayRate);
		}
	}
}

void ATimeThiefCharacterBase::AddOwnedGameplayTag(const FGameplayTag& Tag)
{
	if (Tag.IsValid())
	{
		OwnedGameplayTags.AddTag(Tag);
	}
}

void ATimeThiefCharacterBase::RemoveOwnedGameplayTag(const FGameplayTag& Tag)
{
	if (Tag.IsValid())
	{
		OwnedGameplayTags.RemoveTag(Tag);
	}
}

bool ATimeThiefCharacterBase::HasOwnedGameplayTag(const FGameplayTag& Tag) const
{
	return OwnedGameplayTags.HasTag(Tag);
}

void ATimeThiefCharacterBase::AppendOwnedGameplayTags(const FGameplayTagContainer& InTags)
{
	OwnedGameplayTags.AppendTags(InTags);
}

UMaterialInstanceDynamic* ATimeThiefCharacterBase::GetOrCreateMaterialInstanceDynamic(
	UPrimitiveComponent* MeshComp,
	int32 MaterialIndex)
{
	if (!MeshComp)
	{
		return nullptr;
	}

	UMaterialInterface* CurrentMaterial = MeshComp->GetMaterial(MaterialIndex);

	if (UMaterialInstanceDynamic* ExistingMID =
		Cast<UMaterialInstanceDynamic>(CurrentMaterial))
	{
		return ExistingMID;
	}

	UMaterialInstanceDynamic* NewMID =
		MeshComp->CreateDynamicMaterialInstance(MaterialIndex, CurrentMaterial);

	return NewMID;
}
