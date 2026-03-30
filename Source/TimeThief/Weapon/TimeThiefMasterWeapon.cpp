#include "Weapon/TimeThiefMasterWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "Weapon/Components/TimeThiefRifleComponent.h"
#include "Weapon/Components/TimeThiefShotgunComponent.h"
#include "Weapon/Components/TimeThiefRocketLauncherComponent.h"
#include "TimeThiefGameplayTags.h"

ATimeThiefMasterWeapon::ATimeThiefMasterWeapon() 
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponMesh->SetCastShadow(true);

	const FTimeThiefGameplayTags& GameplayTags = FTimeThiefGameplayTags::Get();

	RifleComponent = CreateDefaultSubobject<UTimeThiefRifleComponent>(TEXT("RifleComponent"));
	RifleComponent->SetWeaponTag(GameplayTags.Weapon_Rifle);

	ShotgunComponent = CreateDefaultSubobject<UTimeThiefShotgunComponent>(TEXT("ShotgunComponent"));
	ShotgunComponent->SetWeaponTag(GameplayTags.Weapon_Shotgun);

	RocketLauncherComponent = CreateDefaultSubobject<UTimeThiefRocketLauncherComponent>(TEXT("RocketLauncherComponent"));
	RocketLauncherComponent->SetWeaponTag(GameplayTags.Weapon_RocketLauncher);
}

void ATimeThiefMasterWeapon::BeginPlay() 
{
	Super::BeginPlay();

	TArray<UTimeThiefWeaponComponentBase*> Components;
	GetComponents<UTimeThiefWeaponComponentBase>(Components);

	for (UTimeThiefWeaponComponentBase* Comp : Components) 
	{
		if (Comp && Comp->GetWeaponTag().IsValid()) 
		{
			WeaponComponents.Add(Comp->GetWeaponTag(), Comp);
		}
	}
}

UStaticMesh* ATimeThiefMasterWeapon::GetActiveStaticMesh() const 
{
	return WeaponMesh ? WeaponMesh->GetStaticMesh() : nullptr;
}

void ATimeThiefMasterWeapon::SwitchWeapon(FGameplayTag WeaponTag) 
{
	if (ActiveWeaponComponent && ActiveWeaponComponent->GetWeaponTag() == WeaponTag) 
	{
		return;
	}

	if (ActiveWeaponComponent) 
	{
		ActiveWeaponComponent->OnUnequipped();
	}

	UTimeThiefWeaponComponentBase* NewWeaponComp = GetWeaponComponentByTag(WeaponTag);
	if (NewWeaponComp) 
	{
		ActiveWeaponComponent = NewWeaponComp;
		
		if (WeaponMesh) 
		{
			WeaponMesh->SetStaticMesh(ActiveWeaponComponent->GetWeaponMeshAsset());
		}

		ActiveWeaponComponent->OnEquipped();
	} 
	else 
	{
		ActiveWeaponComponent = nullptr;
		if (WeaponMesh) 
		{
			WeaponMesh->SetStaticMesh(nullptr);
		}
	}
}

void ATimeThiefMasterWeapon::StartFire() 
{
	if (ActiveWeaponComponent) 
	{
		ActiveWeaponComponent->StartFire();
	}
}

void ATimeThiefMasterWeapon::StopFire() 
{
	if (ActiveWeaponComponent) 
	{
		ActiveWeaponComponent->StopFire();
	}
}

void ATimeThiefMasterWeapon::Reload() 
{
	if (ActiveWeaponComponent) 
	{
		ActiveWeaponComponent->Reload();
	}
}

UTimeThiefWeaponComponentBase* ATimeThiefMasterWeapon::GetWeaponComponentByTag(FGameplayTag WeaponTag) const 
{
	if (const TObjectPtr<UTimeThiefWeaponComponentBase>* FoundComp = WeaponComponents.Find(WeaponTag)) 
	{
		return *FoundComp;
	}
	return nullptr;
}

FTransform ATimeThiefMasterWeapon::GetSocketTransform(FName SocketName, ERelativeTransformSpace TransformSpace) const 
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(SocketName)) 
	{
		return WeaponMesh->GetSocketTransform(SocketName, TransformSpace);
	}
	return GetActorTransform();
}

FVector ATimeThiefMasterWeapon::GetSocketLocation(FName SocketName) const 
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(SocketName)) 
	{
		return WeaponMesh->GetSocketLocation(SocketName);
	}
	return GetActorLocation();
}