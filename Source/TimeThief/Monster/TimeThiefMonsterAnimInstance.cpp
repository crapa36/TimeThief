#include "TimeThiefMonsterAnimInstance.h"
#include "TimeThiefMonster.h"


void UTimeThiefMonsterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	OwnerMonster = Cast<ATimeThiefMonster>(TryGetPawnOwner());
}

void UTimeThiefMonsterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	if (OwnerMonster == nullptr)
	{
		OwnerMonster = Cast<ATimeThiefMonster>(TryGetPawnOwner());
	}

	if (OwnerMonster == nullptr)
	{
		Speed = 0.0f;
		AimYaw = 0.0f;
		AimPitch = 0.0f;
		return;
	}

	Speed = OwnerMonster->GetNetworkSpeed();

	AimYaw = OwnerMonster->GetAimYaw();
	AimPitch = OwnerMonster->GetAimPitch();
	
	// bool bIsDead = OwnerMonster->IsDead();	// 사용 안할듯..?
}
