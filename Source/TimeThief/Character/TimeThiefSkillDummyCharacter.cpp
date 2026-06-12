#include "Character/TimeThiefSkillDummyCharacter.h"

#include "Animation/AnimInstance.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MorphingMesh/MorphingMeshComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"

ATimeThiefSkillDummyCharacter::ATimeThiefSkillDummyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
		Movement->bRunPhysicsWithNoController = true;
	}

	CopiedWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CopiedWeaponMesh"));
	CopiedWeaponMesh->SetupAttachment(GetMesh());
	CopiedWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ATimeThiefSkillDummyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RequestForwardMove(DeltaTime);
}

void ATimeThiefSkillDummyCharacter::InitializeFromSource(ATimeThiefCharacterBase* SourceCharacter, const FVector& InMoveDirection, float InMoveSpeed, float InLifetime)
{
	MoveDirection = InMoveDirection.GetSafeNormal();
	if (MoveDirection.IsNearlyZero())
	{
		MoveDirection = GetActorForwardVector();
	}

	MoveSpeed = FMath::Max(InMoveSpeed, 0.0f);
	if (MoveSpeed <= KINDA_SMALL_NUMBER && SourceCharacter)
	{
		if (const UCharacterMovementComponent* SourceMovement = SourceCharacter->GetCharacterMovement())
		{
			MoveSpeed = SourceMovement->MaxWalkSpeed;
		}
	}

	if (SourceCharacter)
	{
		if (UCapsuleComponent* DummyCapsule = GetCapsuleComponent())
		{
			DummyCapsule->IgnoreActorWhenMoving(SourceCharacter, true);
		}
		if (UCapsuleComponent* SourceCapsule = SourceCharacter->GetCapsuleComponent())
		{
			SourceCapsule->IgnoreActorWhenMoving(this, true);
		}
	}

	ConfigureMovement();

	SetActorRotation(MoveDirection.Rotation());

	if (SourceCharacter)
	{
		ConfigureMeshFromSource(SourceCharacter);
		ConfigureWeaponFromSource(SourceCharacter);
	}

	RequestForwardMove(0.0f);

	if (InLifetime > 0.0f)
	{
		SetLifeSpan(InLifetime);
	}
}

void ATimeThiefSkillDummyCharacter::ConfigureMovement()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	Movement->MaxWalkSpeed = MoveSpeed;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
	Movement->bRunPhysicsWithNoController = true;
	Movement->SetMovementMode(MOVE_Walking);
}

void ATimeThiefSkillDummyCharacter::ConfigureMeshFromSource(ATimeThiefCharacterBase* SourceCharacter)
{
	USkeletalMeshComponent* DummyMesh = GetMesh();

	USkeletalMeshComponent* SourceMesh = SourceCharacter->GetMesh();

	if (const UMorphingMeshComponent* MorphingComponent = SourceCharacter->GetMorphingMeshComponent())
	{
		CopiedMeshAlpha = MorphingComponent->CurrAlpha;
	}

	DummyMesh->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset(), true);
	DummyMesh->SetRelativeTransform(SourceMesh->GetRelativeTransform());

	UClass* SourceAnimClass = SourceMesh->GetAnimClass();
	if (!SourceAnimClass)
	{
		if (const UAnimInstance* AnimInstance = SourceMesh->GetAnimInstance())
		{
			SourceAnimClass = AnimInstance->GetClass();
		}
	}

	if (SourceAnimClass)
	{
		DummyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		DummyMesh->SetAnimInstanceClass(SourceAnimClass);
	}

	DummyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	DummyMesh->EmptyOverrideMaterials();

	for (int32 MaterialIndex = 0; MaterialIndex < SourceMesh->GetNumMaterials(); ++MaterialIndex)
	{
		DummyMesh->SetMaterial(MaterialIndex, SourceMesh->GetMaterial(MaterialIndex));
	}
}

void ATimeThiefSkillDummyCharacter::ConfigureWeaponFromSource(ATimeThiefCharacterBase* SourceCharacter)
{
	ATimeThiefMasterWeapon* SourceWeapon = SourceCharacter->GetWeaponActor();
	UStaticMeshComponent* SourceWeaponMesh = SourceWeapon ? SourceWeapon->GetWeaponMesh() : nullptr;
	if (!SourceWeaponMesh || !SourceWeaponMesh->GetStaticMesh())
	{
		CopiedWeaponMesh->SetStaticMesh(nullptr);
		return;
	}

	CopiedWeaponMesh->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::KeepRelativeTransform,
		SourceWeaponMesh->GetAttachSocketName());
	CopiedWeaponMesh->SetRelativeTransform(SourceWeaponMesh->GetRelativeTransform());
	CopiedWeaponMesh->SetStaticMesh(SourceWeaponMesh->GetStaticMesh());
	CopiedWeaponMesh->EmptyOverrideMaterials();

	for (int32 MaterialIndex = 0; MaterialIndex < SourceWeaponMesh->GetNumMaterials(); ++MaterialIndex)
	{
		CopiedWeaponMesh->SetMaterial(MaterialIndex, SourceWeaponMesh->GetMaterial(MaterialIndex));
	}
}

void ATimeThiefSkillDummyCharacter::RequestForwardMove(float)
{
	AddMovementInput(MoveDirection, 1.0f, true);
}
