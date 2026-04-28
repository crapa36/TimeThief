#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "CharacterTrajectoryComponent.h"
#include "Character/TimeThiefNetworkCharacterBase.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"

UTimeThiefAnimInstance::UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bShouldMove = false;
	bIsFalling = false;
}

void UTimeThiefAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
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

	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
		if (CharacterOwner)
		{
			if (CharacterOwner->GetClass()->ImplementsInterface(UMovableNetworkEntityInterface::StaticClass()))
			{
				MovableNetworkInterface.SetObject(CharacterOwner);
				MovableNetworkInterface.SetInterface(Cast<IMovableNetworkEntityInterface>(CharacterOwner));
			}
			CharacterMovement = CharacterOwner->GetCharacterMovement();
			TrajectoryComponent = CharacterOwner->FindComponentByClass<UCharacterTrajectoryComponent>();
		}
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

	VerticalVelocity = Velocity.Z;
	GroundSpeed = Velocity.Size2D();

	const bool bIsRemoteCharacter = MovableNetworkInterface.GetInterface() && !CharacterOwner->IsLocallyControlled();
	const float MoveSpeedThreshold = bIsRemoteCharacter ? 0.5f : 1.0f;
	bHasVelocity = GroundSpeed > MoveSpeedThreshold;

	const bool bHasAcceleration = !CharacterMovement->GetCurrentAcceleration().IsNearlyZero();
	bShouldMove = bIsRemoteCharacter ? bHasVelocity : ((GroundSpeed > 0.01f) && bHasAcceleration);

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
}
