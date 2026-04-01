#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Weapon/TimeThiefMasterWeapon.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/TimeThiefCharacterBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Network/NetworkCombatSyncComponent.h"

UTimeThiefPawnCombatComponent::UTimeThiefPawnCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UTimeThiefPawnCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	SpawnMasterWeapon();
	
	if (AActor* Owner = GetOwner())
	{
		if (auto NCSC = Owner->FindComponentByClass<UNetworkCombatSyncComponent>())
		{
			NCSC->OnRemoteAttackNotify.AddUObject(this, &UTimeThiefPawnCombatComponent::Remote_AttackRequest);
			
			// TODO: EndPlay나 다른 곳에서 바인딩 해제 할 것
		}
	}
	
}

void UTimeThiefPawnCombatComponent::SpawnMasterWeapon()
{
	if (!MasterWeaponClass || MasterWeaponPtr) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = GetPawn<APawn>();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	MasterWeaponPtr = GetWorld()->SpawnActor<ATimeThiefMasterWeapon>(MasterWeaponClass, SpawnParams);
	if (MasterWeaponPtr)
	{
		AttachMasterWeaponToCharacter(FName("HandGrip_R"));
	}
}

void UTimeThiefPawnCombatComponent::EquipWeapon(FGameplayTag WeaponTag)
{
	if (!MasterWeaponPtr) SpawnMasterWeapon();
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
	MasterWeaponPtr->SwitchWeapon(WeaponTag);
	
	UTimeThiefWeaponComponentBase* CurrentWeapon = MasterWeaponPtr->GetActiveWeaponComponent();
	if (CurrentWeapon)
	{
		MasterWeaponPtr->SetActorHiddenInGame(false);
		AttachMasterWeaponToCharacter(CurrentWeapon->GetSocketName());
		
		ACharacter* OwningCharacter = GetPawn<ACharacter>();
		if (OwningCharacter)
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
				}
			}
		}

		const float MontageLength = PlayEquipMontage(CurrentWeapon);
		if (MontageLength > 0.0f)
		{
			bIsEquippingWeapon = true;
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(EquipTimerHandle, this, &UTimeThiefPawnCombatComponent::OnEquipFinished, MontageLength, false);
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

	USkeletalMeshComponent* TargetMesh = OwningCharacter->GetMesh();
	if (ATimeThiefCharacterBase* BaseChar = Cast<ATimeThiefCharacterBase>(OwningCharacter))
	{
		USkeletalMeshComponent* AttachMesh = BaseChar->GetWeaponAttachMesh();
		if (AttachMesh)
		{
			TargetMesh = AttachMesh;
		}
	}

	MasterWeaponPtr->AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
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
		BaseChar->PlayMontageOnAllMeshes(EquipMontage);
		return EquipMontage->GetPlayLength();
	}

	return 0.0f;
}

void UTimeThiefPawnCombatComponent::OnEquipFinished()
{
	bIsEquippingWeapon = false;
}

UTimeThiefWeaponComponentBase* UTimeThiefPawnCombatComponent::GetCharacterCurrentEquippedWeapon() const
{
	return MasterWeaponPtr ? MasterWeaponPtr->GetActiveWeaponComponent() : nullptr;
}

void UTimeThiefPawnCombatComponent::HandleInputPressed(FGameplayTag InputTag) {}
void UTimeThiefPawnCombatComponent::HandleInputReleased(FGameplayTag InputTag) {}

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
	// TODO: AttackRequest에 맞는 상황에 맞게 멤버 설정 및 애니메이션 재생 될 수 있도록 작성
	
}

void UTimeThiefPawnCombatComponent::Remote_SyncAimingState(bool bNewAiming)
{
	bIsAiming = bNewAiming;
}

void UTimeThiefPawnCombatComponent::Remote_SyncFireAction()
{
	PlayFireMontage();
}

void UTimeThiefPawnCombatComponent::Remote_SyncAimLocation(const FVector& NewAimLocation)
{
	RemoteTargetAimLocation = NewAimLocation;
}

void UTimeThiefPawnCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TargetAimLocation = FMath::VInterpTo(TargetAimLocation, RemoteTargetAimLocation, DeltaTime, 15.f);
}

void UTimeThiefPawnCombatComponent::OnEquipAnimFinished() {}
void UTimeThiefPawnCombatComponent::OnUnequipAnimFinished() {}