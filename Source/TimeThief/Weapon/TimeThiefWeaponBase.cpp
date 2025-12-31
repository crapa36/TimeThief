#include "Weapon/TimeThiefWeaponBase.h"
#include "Components/SkeletalMeshComponent.h"

ATimeThiefWeaponBase::ATimeThiefWeaponBase() {
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
