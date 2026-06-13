#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Components/Skill/TimeThiefSkillComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Network/State/CombatNotifyType.h"
#include "Network/State/RemoteAttackNotify.h"
#include "Network/NetworkCombatSyncComponent.h"
#include "Network/MovableNetworkEntityInterface.h"
#include "TimeThiefGameplayTags.h"
#include "TimeThiefThrowableComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Utils/TimeThiefAimStatics.h"

UTimeThiefPawnCombatComponent::UTimeThiefPawnCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTimeThiefPawnCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		CachedCombatSyncComponent = Owner->GetComponentByClass<UNetworkCombatSyncComponent>();
		if (CachedCombatSyncComponent)
		{
			CachedCombatSyncComponent->OnRemoteAttackNotify.AddUObject(
				this, &UTimeThiefPawnCombatComponent::Remote_AttackRequest);
		}
	}

	if (auto Character = Cast<ATimeThiefCharacterBase>(GetOwner()))
	{
		if (auto MorphingComp = Character->GetMorphingMeshComponent())
		{
			MorphingComp->OnMorphTargetTypeChangedSignature.AddUObject(this, &UTimeThiefPawnCombatComponent::OnChanged);
		}
	}
}

void UTimeThiefPawnCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (CachedCombatSyncComponent)
	{
		CachedCombatSyncComponent->OnRemoteAttackNotify.RemoveAll(this);
		CachedCombatSyncComponent = nullptr;
	}

	if (auto Character = Cast<ATimeThiefCharacterBase>(GetOwner()))
	{
		if (auto MorphingComp = Character->GetMorphingMeshComponent())
		{
			MorphingComp->OnMorphTargetTypeChangedSignature.RemoveAll(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefPawnCombatComponent::OnRegister()
{
	Super::OnRegister();

	if (auto Character = Cast<ATimeThiefCharacterBase>(GetOwner()))
	{
		MasterWeaponPtr = Character->GetWeaponActor();
	}
}

void UTimeThiefPawnCombatComponent::OnChanged(EMorphTargetType Type)
{
	if (UTimeThiefWeaponComponentBase* CurrentWeapon = MasterWeaponPtr->GetActiveWeaponComponent())
	{
		MasterWeaponPtr->SetActorHiddenInGame(false);
		AttachMasterWeaponToCharacter(CurrentWeapon->GetSocketName());
	}
}

void UTimeThiefPawnCombatComponent::EquipWeapon(FGameplayTag WeaponTag)
{
	if (!MasterWeaponPtr) return;

	if (CurrentEquippedWeaponTag == WeaponTag) return;

	if (CurrentEquippedWeaponTag.IsValid())
	{
		UnequipCurrentWeapon();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipTimerHandle);
	}

	CurrentEquippedWeaponTag = WeaponTag;
	MasterWeaponPtr->SetActorHiddenInGame(true);
	MasterWeaponPtr->SwitchWeapon(WeaponTag);

	UTimeThiefWeaponComponentBase* CurrentWeapon = MasterWeaponPtr->GetActiveWeaponComponent();
	if (CurrentWeapon)
	{
		if (auto Player = GetPawn<ATimeThiefCharacterBase>())
		{
			if (!Player->GetMorphingMeshComponent()->bIsSkeletalMesh)
			{
				MasterWeaponPtr->SetActorHiddenInGame(false);
				AttachMasterWeaponToCharacter(CurrentWeapon->GetSocketName());
			}
		}
		
		const float MontageLength = PlayEquipMontage(CurrentWeapon);
		
		if (ACharacter* OwningCharacter = GetPawn<ACharacter>())
		{
			if (TSubclassOf<UAnimInstance> AnimLayer = CurrentWeapon->GetEquipAnimLayer())
			{
				OwningCharacter->GetMesh()->LinkAnimClassLayers(AnimLayer);
				if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
				{
					if (USkeletalMeshComponent* FPMesh = BaseChar->GetFirstPersonMesh())
					{
						FPMesh->LinkAnimClassLayers(AnimLayer);
					}

					if (auto MorphingComp = BaseChar->GetMorphingMeshComponent())
					{
						MorphingComp->MaxMorphingTime = MontageLength;
						MorphingComp->SetType(FTimeThiefGameplayTags::GetMorphTargetTypeByTag(WeaponTag));
					}
				}
			}
		}
		
		if (MontageLength > 0.0f)
		{
			bIsEquippingWeapon = true;
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(EquipTimerHandle, this,&UTimeThiefPawnCombatComponent::OnEquipFinished, MontageLength,false);
			}
		}
		else
		{
			OnEquipFinished();
		}

		ApplyCombatStateTag(WeaponTag);
		OnWeaponEquipped_Delegate.Broadcast(CurrentWeapon);
	}
}

void UTimeThiefPawnCombatComponent::UnequipCurrentWeapon()
{
	if (!MasterWeaponPtr) return;
	UTimeThiefWeaponComponentBase* CurrentWeapon = MasterWeaponPtr->GetActiveWeaponComponent();
	if (!CurrentWeapon) return;

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (OwningCharacter)
	{
		if (UAnimMontage* UnequipMontage = CurrentWeapon->GetUnequipMontage())
		{
			if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
			{
				BaseChar->PlayMontageOnAllMeshes(UnequipMontage);
			}
		}

		if (TSubclassOf<UAnimInstance> AnimLayer = CurrentWeapon->GetEquipAnimLayer())
		{
			OwningCharacter->GetMesh()->UnlinkAnimClassLayers(AnimLayer);
			if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
			{
				if (USkeletalMeshComponent* FPMesh = BaseChar->GetFirstPersonMesh())
				{
					FPMesh->UnlinkAnimClassLayers(AnimLayer);
				}
			}
		}
	}

	RemoveCombatStateTag(CurrentEquippedWeaponTag);
	CurrentEquippedWeaponTag = FGameplayTag();
	MasterWeaponPtr->SwitchWeapon(FGameplayTag::EmptyTag);
	MasterWeaponPtr->SetActorHiddenInGame(true);

	OnWeaponUnequipped_Delegate.Broadcast();
}

void UTimeThiefPawnCombatComponent::AttachMasterWeaponToCharacter(FName SocketName)
{
	if (!MasterWeaponPtr) return;

	ACharacter* OwningCharacter = GetPawn<ACharacter>();
	if (!OwningCharacter) return;

	if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
	{
		if (USkeletalMeshComponent* AttachMesh = BaseChar->GetWeaponAttachMesh())
		{
			if (auto WeaponComp = BaseChar->GetWeaponActor())
			{
				WeaponComp->AttachToComponent(AttachMesh, FAttachmentTransformRules::SnapToTargetIncludingScale,
				                              SocketName);

				// UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Attached MasterWeapon to %s at socket %s"), *AttachMesh->GetSkeletalMeshAsset()->GetName(), *SocketName.ToString()));
			}
		}
	}
}

void UTimeThiefPawnCombatComponent::BroadcastCombatAttackRequest(const FCombatAttackRequest& AttackRequest)
{
	OnCombatAttackRequest_Delegate.Broadcast(AttackRequest);
}

void UTimeThiefPawnCombatComponent::PlayFireMontage()
{
	if (ATimeThiefCharacterBase* Character = Cast<ATimeThiefCharacterBase>(GetOwner()))
	{
		Character->PlayMontageOnAllMeshes(FireMontage);
	}
}

float UTimeThiefPawnCombatComponent::PlayEquipMontage(UTimeThiefWeaponComponentBase* Weapon)
{
	if (!Weapon) return 0.0f;

	UAnimMontage* EquipMontage = Weapon->GetEquipMontage();
	if (!EquipMontage) return 0.0f;

	if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetPawn<ACharacter>()))
	{
		float PlayRate = 1.0f;
		if (const UTimeThiefSkillComponent* SkillComponent = BaseChar->FindComponentByClass<UTimeThiefSkillComponent>())
		{
			PlayRate = SkillComponent->GetEquipSpeedMultiplier();
		}

		BaseChar->PlayMontageOnAllMeshes(EquipMontage, PlayRate);
		return EquipMontage->GetPlayLength() / FMath::Max(PlayRate, 0.01f);
	}

	return 0.0f;
}

FVector UTimeThiefPawnCombatComponent::GetEffectiveShotOrigin() const
{
	if (MasterWeaponPtr)
	{
		if (const UTimeThiefWeaponComponentBase* CurrentWeapon = MasterWeaponPtr->GetActiveWeaponComponent())
		{
			return CurrentWeapon->GetMuzzleLocation();
		}

		return MasterWeaponPtr->GetActorLocation();
	}

	if (const ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		return OwningCharacter->GetPawnViewLocation();
	}

	return FVector::ZeroVector;
}

void UTimeThiefPawnCombatComponent::OnEquipFinished()
{
	bIsEquippingWeapon = false;
}

UTimeThiefWeaponComponentBase* UTimeThiefPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const
{
	return MasterWeaponPtr ? MasterWeaponPtr->GetActiveWeaponComponent() : nullptr;
}

void UTimeThiefPawnCombatComponent::HandleInputPressed(FGameplayTag InputTag)
{
}

void UTimeThiefPawnCombatComponent::HandleInputReleased(FGameplayTag InputTag)
{
}

void UTimeThiefPawnCombatComponent::ApplyCombatStateTag(FGameplayTag WeaponTag)
{
	if (!WeaponTag.IsValid()) return;
	if (const FGameplayTag* StateTag = WeaponToStateTagMap.Find(WeaponTag))
	{
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetPawn<ACharacter>()))
		{
			BaseChar->AddOwnedGameplayTag(*StateTag);
		}
	}
}

void UTimeThiefPawnCombatComponent::RemoveCombatStateTag(FGameplayTag WeaponTag)
{
	if (!WeaponTag.IsValid()) return;
	if (const FGameplayTag* StateTag = WeaponToStateTagMap.Find(WeaponTag))
	{
		if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(GetPawn<ACharacter>()))
		{
			BaseChar->RemoveOwnedGameplayTag(*StateTag);
		}
	}
}

void UTimeThiefPawnCombatComponent::Remote_AttackRequest(const FRemoteAttackNotify& AttackRequest)
{
	if (const APawn* OwningPawn = GetPawn<APawn>())
	{
		if (OwningPawn->IsLocallyControlled())
		{
			// return;
		}
	}

	switch (AttackRequest.NotifyType)
	{
	case ECombatNotifyType::Aiming:
		Remote_SyncAimLocation(AttackRequest.Origin, AttackRequest.Direction);
		Remote_SyncAimingState(true);
		break;
	case ECombatNotifyType::Readying:
		Remote_SyncAimingState(false);
		break;
	case ECombatNotifyType::Fire:
		Remote_SyncAimLocation(AttackRequest.Origin, AttackRequest.Direction);
		CachedRemoteShotSeed = AttackRequest.ShotSeed;
		++RemoteFireNotifyCount;
		if (AttackRequest.WeaponId != 0)
		{
			const FGameplayTag PreviousWeaponTag = CurrentEquippedWeaponTag;
			const FGameplayTag DesiredWeaponTag =
				FTimeThiefGameplayTags::ResolveWeaponTagFromId(AttackRequest.WeaponId);
			if (DesiredWeaponTag.IsValid() && DesiredWeaponTag != CurrentEquippedWeaponTag)
			{
				++RemoteFireWeaponCorrectionCount;
				UE_LOG(LogTemp, Verbose,
				       TEXT("Remote_AttackRequest(Fire): correction=%d/%d from %s to %s (WeaponId=%u)"),
				       RemoteFireWeaponCorrectionCount,
				       RemoteFireNotifyCount,
				       *PreviousWeaponTag.ToString(),
				       *DesiredWeaponTag.ToString(),
				       AttackRequest.WeaponId);
				EquipWeapon(DesiredWeaponTag);
			}
		}
		Remote_SyncFireAction();
		break;
	case ECombatNotifyType::Reload:
		if (UTimeThiefWeaponComponentBase* CurrentWeapon = GetCharacterCurrentEquippedWeapon())
		{
			CurrentWeapon->ExecuteRemoteReload();
		}
		break;
	case ECombatNotifyType::WeaponChange:
		if (const FGameplayTag WeaponTag = FTimeThiefGameplayTags::ResolveWeaponTagFromId(AttackRequest.WeaponId);
			WeaponTag.IsValid())
		{
			EquipWeapon(WeaponTag);
		}
		break;
	case ECombatNotifyType::Throw:
		if (AActor* Owner = GetOwner())
		{
			if (auto ThrowComp = Owner->FindComponentByClass<UTimeThiefThrowableComponent>())
			{
				ThrowComp->RemoteThrowGrenade(AttackRequest);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
				TEXT("Remote_AttackRequest Throw failed: ThrowableComponent missing. Owner=%s"),
				*GetNameSafe(Owner));
			}
		}
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("Remote_AttackRequest: Unknown NotifyType=%d"),
		       static_cast<int32>(AttackRequest.NotifyType));
		break;
	}
}


void UTimeThiefPawnCombatComponent::Remote_SyncAimLocation(const FVector& Origin, const FVector& Direction)
{
	CachedRemoteShotOrigin = Origin;

	if (!Direction.IsNearlyZero())
	{
		CachedRemoteAimDirection = UTimeThiefAimStatics::NormalizeAimDirection(Direction);
		CachedRemoteAimLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(
			Origin,
			CachedRemoteAimDirection,
			10000.0f);
		return;
	}

	if (const ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		FVector ViewLocation = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
		if (const APawn* OwningPawn = Cast<APawn>(OwningCharacter);
			OwningPawn && UTimeThiefAimStatics::ResolveAimView(OwningPawn, ViewLocation, ViewDirection) && !
			ViewDirection.IsNearlyZero())
		{
			CachedRemoteAimDirection = UTimeThiefAimStatics::NormalizeAimDirection(ViewDirection);
			const FVector FallbackOrigin = Origin.IsNearlyZero() ? ViewLocation : Origin;
			CachedRemoteAimLocation = UTimeThiefAimStatics::ResolveAimTargetLocation(
				FallbackOrigin,
				CachedRemoteAimDirection,
				10000.0f);
		}
	}
}

void UTimeThiefPawnCombatComponent::Remote_SyncAimingState(bool bNewAiming)
{
	bIsAiming = bNewAiming;
}

void UTimeThiefPawnCombatComponent::Remote_SyncFireAction()
{
	if (bIsEquippingWeapon)
	{
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UTimeThiefPawnCombatComponent> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick([WeakThis]()
			{
				if (!WeakThis.IsValid())
				{
					return;
				}

				WeakThis->Remote_SyncFireAction();
			});
		}
		return;
	}

	if (ACharacter* OwningCharacter = GetPawn<ACharacter>())
	{
		FVector AimDirection = CachedRemoteAimDirection;
		if (AimDirection.IsNearlyZero() && !CachedRemoteAimLocation.IsNearlyZero())
		{
			AimDirection = UTimeThiefAimStatics::ResolveAimDirectionToTarget(
				GetEffectiveShotOrigin(),
				CachedRemoteAimLocation,
				OwningCharacter->GetActorForwardVector());
		}

		if (!AimDirection.IsNearlyZero())
		{
			const FRotator AimRotation = UTimeThiefAimStatics::ResolveAimRotationFromDirection(
				AimDirection,
				OwningCharacter->GetActorRotation());

			if (ShouldApplyRemoteFireYawRotation())
			{
				OwningCharacter->SetActorRotation(FRotator(0.0f, AimRotation.Yaw, 0.0f));
			}
		}
	}

	if (UTimeThiefWeaponComponentBase* CurrentWeapon = GetCharacterCurrentEquippedWeapon())
	{
		CurrentWeapon->SetRemoteShotSyncData(CachedRemoteShotOrigin, CachedRemoteAimDirection);
		CurrentWeapon->SetRemoteShotSeed(CachedRemoteShotSeed);
		CurrentWeapon->ExecuteRemoteFireShot();
	}

	PlayFireMontage();
}

void UTimeThiefPawnCombatComponent::OnEquipAnimFinished()
{
}

void UTimeThiefPawnCombatComponent::OnUnequipAnimFinished()
{
}
