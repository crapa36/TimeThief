#include "TimeThiefAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Network/MovableNetworkEntityInterface.h"

UTimeThiefAnimInstance::UTimeThiefAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTimeThiefAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
	if (CharacterOwner)
	{
		if (CharacterOwner->GetClass()->ImplementsInterface(UMovableNetworkEntityInterface::StaticClass()))
		{
			MovableNetworkInterface.SetObject(CharacterOwner);
			MovableNetworkInterface.SetInterface(Cast<IMovableNetworkEntityInterface>(CharacterOwner));
		}
		
		CharacterMovement = CharacterOwner->GetCharacterMovement();
	}
}

void UTimeThiefAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	ATimeThiefCharacterBase* CharacterBase = Cast<ATimeThiefCharacterBase>(TryGetPawnOwner());
	if (!CharacterBase || !CharacterMovement)
	{
		return;
	}
	
	if (MovableNetworkInterface.GetInterface())
	{
		Velocity = MovableNetworkInterface->GetNetworkVelocity();
	}
	else
	{
		Velocity = CharacterOwner->GetVelocity();
	}

	VerticalVelocity = Velocity.Z;
	GroundSpeed = Velocity.Size2D();
	bHasVelocity = !FMath::IsNearlyZero(GroundSpeed);
	bIsFalling = CharacterMovement->IsFalling();
	bIsMoving = bHasVelocity && !bIsFalling;

	UTimeThiefPawnCombatComponent* CombatComp = CharacterBase->GetCombatComponent();
	if (CombatComp)
	{
		bIsAiming = CombatComp->IsAiming();
	}
}