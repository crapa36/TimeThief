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
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Utils/TimeThiefAimStatics.h"

UTimeThiefWeaponComponentBase::UTimeThiefWeaponComponentBase()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTimeThiefWeaponComponentBase::BeginPlay()
{
	Super::BeginPlay();
	BaseMaxAmmo = MaxAmmo;
	CurrentAmmo = MaxAmmo;
	CurrentSpread = 0.0f;
	NextAllowedFireTime = FireAnimation ? FMath::Min(FireAnimation->GetPlayLength(), NextAllowedFireTime) : NextAllowedFireTime;
	ReloadTime = ReloadAnimation ? FMath::Min(ReloadAnimation->GetPlayLength(),ReloadTime) : ReloadTime;

	if (auto AnimInstance = Cast<ACharacter>(GetOwner<AActor>()->GetParentActor())->GetMesh()->GetAnimInstance();
		AnimInstance)
	{
		AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &UTimeThiefWeaponComponentBase::OnMontageEnded);
	}

	bWantsToFire = false;
}

void UTimeThiefWeaponComponentBase::SetCapacityBonus(int32 InBonus)
{
	CapacityBonus = FMath::Max(0, InBonus);
	MaxAmmo = FMath::Max(1, BaseMaxAmmo + CapacityBonus);

	if (CurrentAmmo > MaxAmmo)
	{
		CurrentAmmo = MaxAmmo;
	}

	NotifyAmmoChanged();
}

void UTimeThiefWeaponComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UTimeThiefWeaponComponentBase::PlayFireEffects()
{
	const FVector MuzzleLoc = GetMuzzleLocation();
	const FRotator MuzzleRotation = GetSocketTransformByName(GetMuzzleSocketName()).GetRotation().Rotator();

	if (MuzzleFlashEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			this,
			MuzzleFlashEffect,
			MuzzleLoc,
			MuzzleRotation
		);
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLoc);
	}
	if (auto AnimInstance = Cast<ATimeThiefCharacterBase>(GetOwner()->GetParentActor())->GetMesh()->GetAnimInstance();
		AnimInstance && FireAnimation)
	{
		AnimInstance->Montage_Play(FireAnimation);
	}
}

UStaticMeshComponent* UTimeThiefWeaponComponentBase::GetWeaponMeshComponent() const
{
	if (ATimeThiefMasterWeapon* Master = Cast<ATimeThiefMasterWeapon>(GetOwner()))
	{
		return Master->GetWeaponMesh();
	}
	return nullptr;
}

void UTimeThiefWeaponComponentBase::OnEquipped()
{
	if (bWantsToFire)
	{
		StartFire();
	}
}

void UTimeThiefWeaponComponentBase::OnUnequipped()
{
	StopFire();
}

FWeaponStatData UTimeThiefWeaponComponentBase::GetWeaponStatDataForNetwork() const
{
	FWeaponStatData StatData{};
	StatData.MagCapacity = MaxAmmo;
	StatData.FireInterval = GetFireInterval();
	StatData.ReloadTime = ReloadTime;

	return StatData;
}

void UTimeThiefWeaponComponentBase::SetWeaponStatForNetwork(const FWeaponStatData& InStatData)
{
	MaxAmmo = InStatData.MagCapacity;

	const float FireInterval = InStatData.FireInterval;
	FireRate = FireInterval > KINDA_SMALL_NUMBER ? 60.0f / FireInterval : 0.0f; // RPM 계산
	RoundsPerSecond = FireInterval > KINDA_SMALL_NUMBER ? 1.0f / FireInterval : 0.0f; // RPS 계산

	ReloadTime = InStatData.ReloadTime;
	
	NotifyAmmoChanged();
}

void UTimeThiefWeaponComponentBase::StartFire()
{
	bWantsToFire = true;
	if (bIsReloading || bIsFiring) return;
	if (CurrentAmmo <= 0)
	{
		Reload();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now + KINDA_SMALL_NUMBER < NextAllowedFireTime)
		{
			bIsFiring = true;
			const float RemainingDelay = FMath::Max(NextAllowedFireTime - Now, 0.0f);
			World->GetTimerManager().SetTimer(AutoFireTimerHandle, this,
			                                  &UTimeThiefWeaponComponentBase::HandleAutoFireShot, RemainingDelay,
			                                  false);
			return;
		}
	}

	bIsFiring = true;
	HandleAutoFireShot();
}

void UTimeThiefWeaponComponentBase::StopFire()
{
	bWantsToFire = false;
	StopFiringLoop();
}

void UTimeThiefWeaponComponentBase::Reload()
{
	if (!CanReload()) return;
	
	if (CurrentAmmo == 0)
	{
		if (auto AnimInstance = Cast<ATimeThiefCharacterBase>(GetOwner()->GetParentActor())->GetMesh()->GetAnimInstance())
		{
			if (AnimInstance->Montage_IsPlaying(FireAnimation))
			{
				FOnMontageSectionChanged SectionChangedDelegate;
				SectionChangedDelegate.BindWeakLambda(this, [this]
					(UAnimMontage* EndedMontage, FName SectionName, bool bLooped)
				{
					if (EndedMontage == this->FireAnimation && SectionName == FName("Recover_Start"))
					{
						this->bIsReloading = true;
						this->StopFiringLoop();
						this->OnReloadStarted();
						this->BroadcastCombatAttackRequest(ECombatNotifyType::Reload);
					}
				});
			
				AnimInstance->Montage_SetSectionChangedDelegate(SectionChangedDelegate, FireAnimation);
			}
			return;
		}
	}
	
	bIsReloading = true;
	StopFiringLoop();
	OnReloadStarted();
	BroadcastCombatAttackRequest(ECombatNotifyType::Reload);
}

bool UTimeThiefWeaponComponentBase::CanFire() const
{
	return CurrentAmmo > 0 && !bIsReloading;
}

bool UTimeThiefWeaponComponentBase::CanReload() const
{
	return !bIsReloading && CurrentAmmo < MaxAmmo;
}

void UTimeThiefWeaponComponentBase::ExecuteFireShot()
{
}

void UTimeThiefWeaponComponentBase::ExecuteRemoteFireShot()
{
	ExecuteFireShot();
}

void UTimeThiefWeaponComponentBase::ExecuteRemoteReload()
{
	bIsReloading = true;
	StopFiringLoop();
	OnReloadStarted();
}

void UTimeThiefWeaponComponentBase::SetRemoteShotSyncData(const FVector& InOrigin, const FVector& InDirection)
{
	if (InDirection.IsNearlyZero())
	{
		bHasRemoteShotSyncData = false;
		RemoteShotOrigin = FVector::ZeroVector;
		RemoteShotDirection = FVector::ForwardVector;
		return;
	}

	bHasRemoteShotSyncData = true;
	RemoteShotOrigin = InOrigin;
	RemoteShotDirection = InDirection.GetSafeNormal();
}

void UTimeThiefWeaponComponentBase::SetRemoteShotSeed(uint32 InShotSeed)
{
}

void UTimeThiefWeaponComponentBase::OnReloadStarted()
{
	if (auto AnimInstance = Cast<ATimeThiefCharacterBase>(GetOwner()->GetParentActor())->GetMesh()->GetAnimInstance();
		AnimInstance && ReloadAnimation)
	{
		AnimInstance->Montage_Play(ReloadAnimation);
	}
	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetOwner()->GetActorLocation());
	}
}

void UTimeThiefWeaponComponentBase::OnReloadFinished()
{
}

void UTimeThiefWeaponComponentBase::ApplyRecoilAndSpread()
{
}

uint32 UTimeThiefWeaponComponentBase::GetCombatAttackShotSeed() const
{
	return 0;
}

void UTimeThiefWeaponComponentBase::NotifyAmmoChanged()
{
	OnAmmoChanged_Delegate.Broadcast(CurrentAmmo, MaxAmmo);
}

FVector UTimeThiefWeaponComponentBase::GetMuzzleLocation() const
{
	if (UStaticMeshComponent* Mesh = GetWeaponMeshComponent())
	{
		if (Mesh->DoesSocketExist(MuzzleSocketName))
		{
			return Mesh->GetSocketLocation(MuzzleSocketName);
		}
	}
	return GetOwner()->GetActorLocation();
}

void UTimeThiefWeaponComponentBase::HandleAutoFireShot()
{
	if (!bIsFiring || !CanFire())
	{
		StopFiringLoop();
		if (CurrentAmmo <= 0) Reload();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now + KINDA_SMALL_NUMBER < NextAllowedFireTime)
		{
			const float RemainingDelay = FMath::Max(NextAllowedFireTime - Now, 0.0f);
			World->GetTimerManager().SetTimer(AutoFireTimerHandle, this,
			                                  &UTimeThiefWeaponComponentBase::HandleAutoFireShot, RemainingDelay,
			                                  false);
			return;
		}
	}

	CurrentAmmo--;
	NotifyAmmoChanged();
	ExecuteFireShot();
	BroadcastCombatAttackRequest(ECombatNotifyType::Fire);
	ApplyRecoilAndSpread();
	if (UWorld* World = GetWorld())
	{
		NextAllowedFireTime = World->GetTimeSeconds() + GetFireInterval();
	}
	if (CurrentAmmo <= 0)
	{
		StopFiringLoop();
		Reload();
	}
	else if (bWantsToFire)
	{
		if (UWorld* World = GetWorld())
		{
			const float Delay = FMath::Max(NextAllowedFireTime - World->GetTimeSeconds(), 0.0f);
			World->GetTimerManager().SetTimer(AutoFireTimerHandle, this,
			                                  &UTimeThiefWeaponComponentBase::HandleAutoFireShot, Delay, false);
		}
	}
}

void UTimeThiefWeaponComponentBase::HandleReloadResult(uint32 DeltaAmmo, uint32 NewAmmo)
{
	const int32 ServerAmmo = FMath::Clamp(static_cast<int32>(NewAmmo), 0, MaxAmmo);
	if (ServerAmmo != static_cast<int32>(NewAmmo))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("HandleReloadResult: Server ammo is out of range. MaxAmmo: %d, ServerAmmo: %u"),
		       MaxAmmo, NewAmmo);
	}

	if ((ServerAmmo - CurrentAmmo) != static_cast<int32>(DeltaAmmo))
	{
		// 이건 경고
		UE_LOG(LogTemp, Warning,
		       TEXT("HandleReloadResult: DeltaAmmo does not match the expected value. Expected: %d, Actual: %u"),
		       ServerAmmo - CurrentAmmo, DeltaAmmo);
	}

	FinishReloadWithAmmo(ServerAmmo);
}

float UTimeThiefWeaponComponentBase::GetFireInterval() const
{
	return RoundsPerSecond > 0.0f ? 1.0f / RoundsPerSecond : (FireRate > 0.0f ? 60.0f / FireRate : 0.1f);
}

void UTimeThiefWeaponComponentBase::StopFiringLoop()
{
	bIsFiring = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
}

void UTimeThiefWeaponComponentBase::FinishReload()
{
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		if (!NGIS->IsConnected())
		{
			CurrentAmmo = MaxAmmo;
			NotifyAmmoChanged();
		}
	}
	
	bIsReloading = false;
	OnReloadFinished();
	if (bWantsToFire) StartFire();
}

void UTimeThiefWeaponComponentBase::FinishReloadWithAmmo(int32 NewAmmo)
{
	CurrentAmmo = FMath::Clamp(NewAmmo, 0, MaxAmmo);
	NotifyAmmoChanged();
}

void UTimeThiefWeaponComponentBase::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == ReloadAnimation)
	{
		// if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
		// {
		// 	if (NGIS->IsConnected())
		// 	{
		// 		return;
		// 	}
		// }

		FinishReload();
	}
}

void UTimeThiefWeaponComponentBase::BroadcastCombatAttackRequest(ECombatNotifyType NotifyType) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner() ? GetOwner()->GetParentActor() : nullptr);
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

	if (NotifyType == ECombatNotifyType::Fire)
	{
		if (!TryGetLastShotSyncData(Request.Origin, Request.Direction))
		{
			Request.Origin = GetLocalAttackOrigin();
			Request.Direction = GetLocalAttackDirection();
		}
		Request.ShotSeed = GetCombatAttackShotSeed();
	}
	else if (NotifyType == ECombatNotifyType::Throw)
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
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner() ? GetOwner()->GetParentActor() : nullptr))
	{
		FVector ViewLocation = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
		if (UTimeThiefAimStatics::ResolveAimView(OwnerPawn, ViewLocation, ViewDirection))
		{
			return ViewDirection;
		}
	}

	return FVector::ForwardVector;
}

bool UTimeThiefWeaponComponentBase::ResolveFireAimView(FVector& OutViewLocation, FVector& OutViewDirection) const
{
	if (bHasRemoteShotSyncData && !RemoteShotDirection.IsNearlyZero())
	{
		OutViewLocation = RemoteShotOrigin;
		OutViewDirection = UTimeThiefAimStatics::NormalizeAimDirection(RemoteShotDirection);
		return true;
	}

	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner() ? GetOwner()->GetParentActor() : nullptr))
	{
		return UTimeThiefAimStatics::ResolveAimView(OwnerPawn, OutViewLocation, OutViewDirection);
	}

	OutViewLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	OutViewDirection = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
	OutViewDirection = UTimeThiefAimStatics::NormalizeAimDirection(OutViewDirection);
	return true;
}

void UTimeThiefWeaponComponentBase::CacheLastShotSyncData(const FVector& InOrigin, const FVector& InDirection)
{
	if (InDirection.IsNearlyZero())
	{
		bHasLastShotSyncData = false;
		LastShotOrigin = FVector::ZeroVector;
		LastShotDirection = FVector::ForwardVector;
		return;
	}

	bHasLastShotSyncData = true;
	LastShotOrigin = InOrigin;
	LastShotDirection = UTimeThiefAimStatics::NormalizeAimDirection(InDirection);
}

bool UTimeThiefWeaponComponentBase::TryGetLastShotSyncData(FVector& OutOrigin, FVector& OutDirection) const
{
	if (!bHasLastShotSyncData)
	{
		return false;
	}

	OutOrigin = LastShotOrigin;
	OutDirection = LastShotDirection;
	return !OutDirection.IsNearlyZero();
}

FTransform UTimeThiefWeaponComponentBase::GetSocketTransformByName(FName InSocketName) const
{
	if (UStaticMeshComponent* Mesh = GetWeaponMeshComponent())
	{
		if (Mesh->DoesSocketExist(InSocketName))
		{
			return Mesh->GetSocketTransform(InSocketName);
		}
	}
	return GetOwner()->GetActorTransform();
}
