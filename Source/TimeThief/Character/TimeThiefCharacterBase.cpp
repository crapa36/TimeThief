#include "Character/TimeThiefCharacterBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATimeThiefCharacterBase::ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetActive(false);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetVisibility(false);

	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->bCastHiddenShadow = true;
	
	bIsFirstPerson = false;
}

void ATimeThiefCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (bIsFirstPerson)
	{
		FirstPersonCamera->SetActive(true);
		FirstPersonMesh->SetVisibility(true);
		FirstPersonMesh->SetOnlyOwnerSee(true);
		GetMesh()->SetOwnerNoSee(true);
	}
	else
	{
		FirstPersonCamera->SetActive(false);
		FirstPersonMesh->SetVisibility(false);
		GetMesh()->SetOwnerNoSee(false);
	}
}

void ATimeThiefCharacterBase::TogglePerspective()
{
	bIsFirstPerson = !bIsFirstPerson;

	if (bIsFirstPerson)
	{
		FirstPersonCamera->SetActive(true);
		FirstPersonMesh->SetVisibility(true);
		FirstPersonMesh->SetOnlyOwnerSee(true);
		
		GetMesh()->SetOwnerNoSee(true);
	}
	else
	{
		FirstPersonCamera->SetActive(false);
		FirstPersonMesh->SetVisibility(false);
		
		GetMesh()->SetOwnerNoSee(false);
	}
}
