#include "Weapon/TimeThiefWeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Logging/StructuredLog.h"

ATimeThiefWeaponBase::ATimeThiefWeaponBase() {
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SocketName = FName("WeaponSocket");
}

void ATimeThiefWeaponBase::BeginPlay() {
	Super::BeginPlay();

	if (!WeaponMesh->GetSkeletalMeshAsset()) {
		UE_LOGFMT(LogActor, Warning, "Weapon {Name} has no Skeletal Mesh assigned!", GetName());
	}
}

void ATimeThiefWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Unequip();
	Super::EndPlay(EndPlayReason);
}

void ATimeThiefWeaponBase::Equip(USceneComponent* InParent, FName InSocketName, AActor* NewOwner, APawn* NewInstigator) {
	if (!InParent || !NewOwner) {
		UE_LOGFMT(LogActor, Error, "Equip failed: Invalid Parent or Owner passed to {Name}", GetName());
		return;
	}

	SetOwner(NewOwner);
	SetInstigator(NewInstigator);

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(InParent, AttachmentRules, InSocketName);

	IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(NewOwner);
	if (AbilityInterface) {
		UAbilitySystemComponent* ASC = AbilityInterface->GetAbilitySystemComponent();
		if (ASC && HasAuthority()) {
			for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities) {
				if (AbilityClass) {
					FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
					FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
					GrantedAbilityHandles.Add(Handle);
				}
			}
		}
	}
}

void ATimeThiefWeaponBase::Unequip() {
	if (GrantedAbilityHandles.Num() > 0) {
		IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(GetOwner());
		if (AbilityInterface) {
			UAbilitySystemComponent* ASC = AbilityInterface->GetAbilitySystemComponent();
			if (ASC && HasAuthority()) {
				for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles) {
					ASC->ClearAbility(Handle);
				}
			}
		}
		GrantedAbilityHandles.Empty();
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetOwner(nullptr);
	SetInstigator(nullptr);
}