#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequenceBase.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Network/State/CombatAttackRequest.h"
#include "Network/State/CombatNotifyType.h"
#include "TimeThiefGameplayTags.h"

UTimeThiefWeaponComponentBase::UTimeThiefWeaponComponentBase() {
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UTimeThiefWeaponComponentBase::BeginPlay() {
	Super::BeginPlay();
	CurrentAmmo = MaxAmmo;
	CurrentSpread = 0.0f;
	NextAllowedFireTime = 0.0f;
	bWantsToFire = false;
}

void UTimeThiefWeaponComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefWeaponComponentBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

UStaticMeshComponent* UTimeThiefWeaponComponentBase::GetWeaponMeshComponent() const {
	if (ATimeThiefMasterWeapon* Master = Cast<ATimeThiefMasterWeapon>(GetOwner())) {
		return Master->GetWeaponMesh();
	}
	return nullptr;
}

void UTimeThiefWeaponComponentBase::OnEquipped() {
	if (bWantsToFire) {
		StartFire();
	}
}

void UTimeThiefWeaponComponentBase::OnUnequipped() {
	StopFire();
}

void UTimeThiefWeaponComponentBase::StartFire() {
	bWantsToFire = true;
	if (bIsReloading || bIsFiring) return;
	if (CurrentAmmo <= 0) {
		Reload();
		return;
	}
	bIsFiring = true;
	HandleAutoFireShot();
}

void UTimeThiefWeaponComponentBase::StopFire() {
	bWantsToFire = false;
	StopFiringLoop();
}

void UTimeThiefWeaponComponentBase::Reload() {
	if (!CanReload()) return;
	bIsReloading = true;
	StopFiringLoop();
	OnReloadStarted();
	BroadcastCombatAttackRequest(ECombatNotifyType::Reload);
	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UTimeThiefWeaponComponentBase::FinishReload, ReloadTime, false);
	}
}

bool UTimeThiefWeaponComponentBase::CanFire() const {
	return CurrentAmmo > 0 && !bIsReloading;
}

bool UTimeThiefWeaponComponentBase::CanReload() const {
	return !bIsReloading && CurrentAmmo < MaxAmmo;
}

void UTimeThiefWeaponComponentBase::ExecuteFireShot() {}

void UTimeThiefWeaponComponentBase::ExecuteRemoteFireShot()
{
	ExecuteFireShot();
}

void UTimeThiefWeaponComponentBase::OnReloadStarted() {
	if (ReloadAnimation) {
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetOwner()->GetOwner())) {
			BaseChar->PlayAnimationOnAllMeshes(ReloadAnimation, WeaponAnimSlot);
		}
	}
	if (ReloadSound) {
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetOwner()->GetActorLocation());
	}
}

void UTimeThiefWeaponComponentBase::OnReloadFinished() {}

void UTimeThiefWeaponComponentBase::ApplyRecoilAndSpread() {}

void UTimeThiefWeaponComponentBase::NotifyAmmoChanged() {
	OnAmmoChanged_Delegate.Broadcast(CurrentAmmo, MaxAmmo);
}

FVector UTimeThiefWeaponComponentBase::GetMuzzleLocation() const {
	if (UStaticMeshComponent* Mesh = GetWeaponMeshComponent()) {
		if (Mesh->DoesSocketExist(MuzzleSocketName)) {
			return Mesh->GetSocketLocation(MuzzleSocketName);
		}
	}
	return GetOwner()->GetActorLocation();
}

void UTimeThiefWeaponComponentBase::HandleAutoFireShot() {
	if (!bIsFiring || !CanFire()) {
		StopFiringLoop();
		if (CurrentAmmo <= 0) Reload();
		return;
	}
	CurrentAmmo--;
	NotifyAmmoChanged();
	ExecuteFireShot();
	BroadcastCombatAttackRequest(ECombatNotifyType::Fire);
	ApplyRecoilAndSpread();
	if (CurrentAmmo <= 0) {
		StopFiringLoop();
		Reload();
	} else if (bWantsToFire) {
		if (UWorld* World = GetWorld()) {
			World->GetTimerManager().SetTimer(AutoFireTimerHandle, this, &UTimeThiefWeaponComponentBase::HandleAutoFireShot, GetFireInterval(), false);
		}
	}
}

void UTimeThiefWeaponComponentBase::StopFiringLoop() {
	bIsFiring = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
}

float UTimeThiefWeaponComponentBase::GetFireInterval() const {
	return RoundsPerSecond > 0.0f ? 1.0f / RoundsPerSecond : (FireRate > 0.0f ? 60.0f / FireRate : 0.1f);
}

void UTimeThiefWeaponComponentBase::FinishReload() {
	CurrentAmmo = MaxAmmo;
	bIsReloading = false;
	NotifyAmmoChanged();
	OnReloadFinished();
	if (bWantsToFire) StartFire();
}

void UTimeThiefWeaponComponentBase::BroadcastCombatAttackRequest(ECombatNotifyType NotifyType) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner() ? GetOwner()->GetOwner() : nullptr);
	if (!OwnerPawn)
	{
		return;
	}

	UTimeThiefPawnCombatComponent* CombatComponent = OwnerPawn->FindComponentByClass<UTimeThiefPawnCombatComponent>();
	if (!CombatComponent)
	{
		return;
	}

	FCombatAttackRequest Request{};
	Request.NotifyType = NotifyType;
	Request.WeaponId = FTimeThiefGameplayTags::ResolveWeaponIdFromTag(WeaponTag);

	if (NotifyType == ECombatNotifyType::Fire || NotifyType == ECombatNotifyType::Throw)
	{
		Request.Origin = GetLocalAttackOrigin();
		Request.Direction = GetLocalAttackDirection();
	}

	CombatComponent->BroadcastCombatAttackRequest(Request);
}

FVector UTimeThiefWeaponComponentBase::GetLocalAttackOrigin() const
{
	return GetMuzzleLocation();
}

FVector UTimeThiefWeaponComponentBase::GetLocalAttackDirection() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner() ? GetOwner()->GetOwner() : nullptr))
	{
		if (const APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
			return CameraRotation.Vector().GetSafeNormal();
		}

		return OwnerPawn->GetBaseAimRotation().Vector().GetSafeNormal();
	}

	return FVector::ForwardVector;
}

FTransform UTimeThiefWeaponComponentBase::GetSocketTransformByName(FName InSocketName) const {
	if (UStaticMeshComponent* Mesh = GetWeaponMeshComponent()) {
		if (Mesh->DoesSocketExist(InSocketName)) {
			return Mesh->GetSocketTransform(InSocketName);
		}
	}
	return GetOwner()->GetActorTransform();
}