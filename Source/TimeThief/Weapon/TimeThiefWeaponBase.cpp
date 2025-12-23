#include "Weapon/TimeThiefWeaponBase.h"
#include "Components/SkeletalMeshComponent.h"

ATimeThiefWeaponBase::ATimeThiefWeaponBase() {
	PrimaryActorTick.bCanEverTick = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATimeThiefWeaponBase::BeginPlay() {
	Super::BeginPlay();
}

void ATimeThiefWeaponBase::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}