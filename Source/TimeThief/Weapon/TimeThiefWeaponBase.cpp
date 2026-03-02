#include "Weapon/TimeThiefWeaponBase.h"
#include "Components/StaticMeshComponent.h"

ATimeThiefWeaponBase::ATimeThiefWeaponBase() {
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	WeaponMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

FTransform ATimeThiefWeaponBase::GetSocketTransformByName(FName InSocketName) const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(InSocketName))
	{
		return WeaponMesh->GetSocketTransform(InSocketName);
	}
	return GetActorTransform();
}
