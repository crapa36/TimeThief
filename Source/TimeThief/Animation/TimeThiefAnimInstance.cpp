#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefSkillDummyCharacter.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "CharacterTrajectoryComponent.h"
#include "Character/TimeThiefNetworkCharacterBase.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Weapon/TimeThiefMasterWeapon.h"

UTimeThiefAnimInstance::UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldMove = false;
	bIsFalling = false;
}

void UTimeThiefAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CacheCharacterReferences();
}

void UTimeThiefAnimInstance::CacheCharacterReferences()
{
	CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
	MovableNetworkInterface.SetObject(nullptr);
	MovableNetworkInterface.SetInterface(nullptr);
	CharacterMovement = nullptr;
	TrajectoryComponent = nullptr;

	if (!CharacterOwner)
	{
		return;
	}

	if (CharacterOwner->GetClass()->ImplementsInterface(UMovableNetworkEntityInterface::StaticClass()))
	{
		MovableNetworkInterface.SetObject(CharacterOwner);
		MovableNetworkInterface.SetInterface(Cast<IMovableNetworkEntityInterface>(CharacterOwner));
	}

	CharacterMovement = CharacterOwner->GetCharacterMovement();
	TrajectoryComponent = CharacterOwner->FindComponentByClass<UCharacterTrajectoryComponent>();
}

void UTimeThiefAnimInstance::TriggerDoubleJump()
{
	bIsDoubleJumping = true;
}

void UTimeThiefAnimInstance::OnEnterDoubleJumpStart(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	bIsDoubleJumping = false;
}

void UTimeThiefAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (CharacterOwner.Get() != TryGetPawnOwner() || !CharacterMovement)
	{
		CacheCharacterReferences();
	}

	if (!CharacterOwner || !CharacterMovement)
	{
		return;
	}

	if (MovableNetworkInterface.GetInterface())
	{
		if (!CharacterOwner->IsLocallyControlled())
		{
			const FVector2D NetworkVelocity2D = MovableNetworkInterface->GetNetworkVelocity2D();
			Velocity = FVector(NetworkVelocity2D.X, NetworkVelocity2D.Y, Velocity.Z);
			if (auto NetPlayer = Cast<ATimeThiefNetworkCharacterBase>(CharacterOwner))
			{
				bIsFalling = NetPlayer->bIsJumping;
			}
		}
		else
		{
			Velocity = CharacterOwner->GetVelocity();
			bIsFalling = CharacterMovement->IsFalling();
		}
	}
	else
	{
		Velocity = CharacterOwner->GetVelocity();
		bIsFalling = CharacterMovement->IsFalling();
	}

	const ATimeThiefSkillDummyCharacter* SkillDummyOwner = Cast<ATimeThiefSkillDummyCharacter>(CharacterOwner);
	if (SkillDummyOwner && Velocity.IsNearlyZero())
	{
		Velocity = SkillDummyOwner->GetIntendedMoveVelocity();
	}

	VerticalVelocity = Velocity.Z;
	GroundSpeed = Velocity.Size2D();

	const bool bIsRemoteCharacter = MovableNetworkInterface.GetInterface() && !CharacterOwner->IsLocallyControlled();
	const bool bIsSkillDummy = SkillDummyOwner != nullptr;
	const bool bUsesVelocityOnlyMove = bIsRemoteCharacter || bIsSkillDummy;
	const float MoveSpeedThreshold = bUsesVelocityOnlyMove ? 0.5f : 1.0f;
	bHasVelocity = GroundSpeed > MoveSpeedThreshold;

	if (bUsesVelocityOnlyMove)
	{
		bShouldMove = bHasVelocity;
	}
	else
	{
		const bool bHasAcceleration = !CharacterMovement->GetCurrentAcceleration().IsNearlyZero();
		bShouldMove = (GroundSpeed > 0.01f) && bHasAcceleration;
	}

	bIsMoving = bHasVelocity && !bIsFalling;

	if (bIsMoving)
	{
		auto Rotation = CharacterOwner->GetActorRotation();
		auto RotFromX = UKismetMathLibrary::MakeRotFromX(Velocity);

		auto DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(RotFromX, Rotation);
		Direction = DeltaRotation.Yaw;
	}
	else
	{
		Direction = 0;
	}

	if (ATimeThiefCharacterBase* CharacterBase = Cast<ATimeThiefCharacterBase>(CharacterOwner))
	{
		if (auto Weapon = CharacterBase->GetWeaponActor())
		{
			if (auto Mesh = Weapon->GetWeaponMesh())
			{
				WeaponSocket = Mesh->GetSocketTransform(FName("LeftHandIK"));
			}
		}
		if (auto MorphingComp = CharacterBase->GetMorphingMeshComponent())
		{
			MeshAlpha = MorphingComp->CurrAlpha;
		}
		
		if (auto Combat = Cast<UTimeThiefPlayerCombatComponent>(CharacterBase->GetCombatComponent()))
		{
			TurnDirection = Combat->TurnDirection;
		}
	}
	else if (SkillDummyOwner)
	{
		MeshAlpha = SkillDummyOwner->GetCopiedMeshAlpha();
		TurnDirection = 0;
	}
}
