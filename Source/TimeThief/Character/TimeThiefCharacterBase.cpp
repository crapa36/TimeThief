#include "Character/TimeThiefCharacterBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATimeThiefCharacterBase::ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->bCastHiddenShadow = true;

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

		if (bIsFirstPerson)
		{
			FirstPersonCamera->SetActive(true);
			GetMesh()->SetOwnerNoSee(true);
			if (FirstPersonMesh)
			{
				FirstPersonMesh->SetVisibility(true);
			}
		}
		else
		{
			FirstPersonCamera->SetActive(false);
			GetMesh()->SetOwnerNoSee(false);
			if (FirstPersonMesh)
			{
				FirstPersonMesh->SetVisibility(false);
			}
		}
	}
}

void ATimeThiefCharacterBase::TogglePerspective()
{
	bIsFirstPerson = !bIsFirstPerson;

	if (IsLocallyControlled())
	{
		if (bIsFirstPerson)
		{
			FirstPersonCamera->SetActive(true);
			GetMesh()->SetOwnerNoSee(true);
			if (FirstPersonMesh)
			{
				FirstPersonMesh->SetVisibility(true);
			}
		}
		else
		{
			FirstPersonCamera->SetActive(false);
			GetMesh()->SetOwnerNoSee(false);
			if (FirstPersonMesh)
			{
				FirstPersonMesh->SetVisibility(false);
			}
		}
	}
}
