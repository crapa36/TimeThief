#include "Character/TimeThiefCharacterBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/System/TimePointSystemComponent.h"
#include "StoreCommons.h"
#include "TimeThiefPlayerState.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

ATimeThiefCharacterBase::ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HealthComponent = CreateDefaultSubobject<UTimeThiefHealthComponent>(TEXT("HealthComponent"));

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

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
}

bool ATimeThiefCharacterBase::PurchaseItem(const FStoreOrder& Order)
{
	if (TimePointSystemComponent->GetTimePoints() >= Order.Price)
	{
		TimePointSystemComponent->ModifyTimePoints(-Order.Price);
		ATimeThiefPlayerState* PS = Cast<ATimeThiefPlayerState>(GetPlayerState());
			switch (Order.ItemName)
			{
			case EStoreItemName::DamageUpgrade:
				PS->Status.Damage++;
				break;
			case EStoreItemName::StabilityUpgrade:
				PS->Status.Stability++;
				break;
			case EStoreItemName::CapacityUpgrade:
				PS->Status.Capacity++;
				break;
			case EStoreItemName::HealthUpgrade:
				PS->Status.Health++;
				break;
			case EStoreItemName::SpeedUpgrade:
				PS->Status.Speed++;
				break;
			default:
				break;
			}
		return true;
	}
	
	return false;
}

void ATimeThiefCharacterBase::BeginPlay()
{
	Super::BeginPlay();

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

