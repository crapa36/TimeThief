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
#include "TimeThiefPlayerState.h"
#include "ViewportInteractionTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/TimeThiefPawnExtensionComponent.h"
#include "Components/System/InventorySystemComponent.h"

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
}

void ATimeThiefCharacterBase::SetMask(float NewMask)
{
	Mask = std::clamp(NewMask, 0.2f, 1.f);
	
	UpdateMask();
}

void ATimeThiefCharacterBase::AddMask(float Amount)
{
	Mask = std::clamp(Mask + Amount, 0.2f, 1.f);
	
	UpdateMask();
}

void ATimeThiefCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
	const auto& Materials = GetMesh()->GetMaterials();
	for (int i = 0;i < Materials.Num(); ++i)
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Materials[i], this))
		{
			GetMesh()->SetMaterial(i, MID);
		}
	}
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
	GetCharacterMovement()->bOrientRotationToMovement = !bIsFirstPerson;
}

void ATimeThiefCharacterBase::UpdateMask()
{
	for (auto Material : GetMesh()->GetMaterials())
	{
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material))
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

	if (UAnimInstance* ThirdPersonAnim = GetMesh()->GetAnimInstance())
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

void ATimeThiefCharacterBase::PlayAnimationOnAllMeshes(UAnimSequenceBase* Animation, FName SlotName, float BlendInTime, float BlendOutTime, float PlayRate)
{
	if (!Animation)
	{
		return;
	}

	if (UAnimInstance* ThirdPersonAnim = GetMesh()->GetAnimInstance())
	{
		ThirdPersonAnim->PlaySlotAnimationAsDynamicMontage(Animation, SlotName, BlendInTime, BlendOutTime, PlayRate);
	}

	if (FirstPersonMesh)
	{
		if (UAnimInstance* FirstPersonAnim = FirstPersonMesh->GetAnimInstance())
		{
			FirstPersonAnim->PlaySlotAnimationAsDynamicMontage(Animation, SlotName, BlendInTime, BlendOutTime, PlayRate);
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