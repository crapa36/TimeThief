#include "Components/Combat/TimeThiefThrowableComponent.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefPlayerCharacter.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/System/InventorySystemComponent.h"
#include "DataAssets/TimeThiefThrowableData.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimeThiefGameplayTags.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "Network/State/CombatAttackRequest.h"
#include "Network/State/CombatNotifyType.h"
#include "Network/State/RemoteAttackNotify.h"
#include "Utils/TimeThiefAimStatics.h"
#include "Weapon/TimeThiefThrowableProjectile.h"

UTimeThiefThrowableComponent::UTimeThiefThrowableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ThrowableProjectileClass = ATimeThiefThrowableProjectile::StaticClass();
}

void UTimeThiefThrowableComponent::HandleInputPressed(FGameplayTag InputTag)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] HandleInputPressed Tag=%s Expected=%s Owner=%s"),
		*InputTag.ToString(),
		*FTimeThiefGameplayTags::Get().InputTag_Action_Throw.ToString(),
		*GetNameSafe(GetOwner()));
#endif

	if (InputTag == FTimeThiefGameplayTags::Get().InputTag_Action_Throw)
	{
		const bool bThrown = TryThrowEquippedThrowable();

#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] TryThrowEquippedThrowable Result=%s"),
			bThrown ? TEXT("Success") : TEXT("Failed"));
#endif
	}
}

bool UTimeThiefThrowableComponent::TryThrowEquippedThrowable()
{
	ATimeThiefPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	UInventorySystemComponent* InventoryComponent = GetInventoryComponent();
	if (!PlayerCharacter || !InventoryComponent)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] Throw failed: PlayerCharacter=%s Inventory=%s Owner=%s"),
			*GetNameSafe(PlayerCharacter),
			*GetNameSafe(InventoryComponent),
			*GetNameSafe(GetOwner()));
#endif
		return false;
	}

	const EItemID ThrowableItem = InventoryComponent->GetThrowableEquipment();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] EquippedThrowable=%d Quantity=%d"),
		static_cast<int32>(ThrowableItem),
		InventoryComponent->GetItemQuantity(ThrowableItem));
#endif

	if (!CanThrowItem(ThrowableItem))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] Throw failed: CanThrowItem returned false. ItemID=%d"),
			static_cast<int32>(ThrowableItem));
#endif
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || !ThrowableProjectileClass)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] Throw failed: World=%s ProjectileClass=%s"),
			World ? TEXT("Valid") : TEXT("Null"),
			*GetNameSafe(ThrowableProjectileClass));
#endif
		return false;
	}

	const FTimeThiefThrowableDefinition ThrowableDefinition = ResolveThrowableDefinition(ThrowableItem);
	const FTimeThiefThrowableThrowSettings& ThrowSettings = ThrowableDefinition.ThrowSettings;
	const FVector ThrowOrigin = ResolveThrowOrigin(PlayerCharacter);
	const FVector ThrowVelocity = ResolveThrowVelocity(PlayerCharacter, ThrowOrigin, ThrowSettings);
	if (ThrowVelocity.IsNearlyZero())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] Throw failed: zero throw velocity. Origin=%s"),
			*ThrowOrigin.ToCompactString());
#endif
		return false;
	}

	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		if (!NGIS->IsConnected())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = PlayerCharacter;
			SpawnParams.Instigator = PlayerCharacter;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			const FTransform SpawnTransform(ThrowVelocity.Rotation(), ThrowOrigin);
			ATimeThiefThrowableProjectile* Projectile = World->SpawnActor<ATimeThiefThrowableProjectile>(
				ThrowableProjectileClass,
				SpawnTransform,
				SpawnParams);

			if (!Projectile)
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] Throw failed: projectile spawn returned null. Origin=%s Velocity=%s"),
					*ThrowOrigin.ToCompactString(),
					*ThrowVelocity.ToCompactString());
#endif
				return false;
			}

			Projectile->InitializeThrowable(ThrowableItem, PlayerCharacter, PlayerCharacter, ThrowableDefinition.ProjectileSettings);
			Projectile->SetThrowableMesh();
			Projectile->LaunchThrowable(ThrowVelocity, ThrowSettings.FuseTime);
		}
	}

	if (UTimeThiefPawnCombatComponent* CombatComponent = PlayerCharacter->FindComponentByClass<UTimeThiefPawnCombatComponent>())
	{
		FCombatAttackRequest Request{};
		Request.NotifyType = ECombatNotifyType::Throw;
		// Request.WeaponId = ResolveGrenadeTypeForFutureNetwork(ThrowableItem);
		Request.WeaponId = static_cast<uint32>(ThrowableItem);
		Request.Origin = ThrowOrigin;
		Request.Direction = ThrowVelocity.GetSafeNormal();
		CombatComponent->BroadcastCombatAttackRequest(Request);
	}

	PlayThrowAnimation(PlayerCharacter, ThrowSettings);
	PlayThrowSound(ThrowSettings, ThrowOrigin);

	// Temporarily disabled for local throw testing without inventory counts.
	// if (!InventoryComponent->RemoveItem(ThrowableItem, 1))
	// {
	// 	Projectile->Destroy();
	// 	return false;
	// }

	NextAllowedThrowTime = World->GetTimeSeconds() + ThrowSettings.ThrowCooldown;

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("[Throwable][Local] Threw ItemID=%d FutureGrenadeType=%u Origin=%s Velocity=%s"),
		static_cast<int32>(ThrowableItem),
		ResolveGrenadeTypeForFutureNetwork(ThrowableItem),
		*ThrowOrigin.ToCompactString(),
		*ThrowVelocity.ToCompactString());
#endif

	return true;
}

bool UTimeThiefThrowableComponent::CanThrowEquippedThrowable() const
{
	return CanThrowItem(GetEquippedThrowableItem());
}

void UTimeThiefThrowableComponent::RemoteThrowGrenade(const FRemoteAttackNotify& Notify)
{
	UE_LOG(LogTemp, Warning, TEXT("RemoteThrowGrenade called: AttackerEntityId=%u SpawnEntityId=%u WeaponId=%u Origin=%s Direction=%s"),
		Notify.AttackerEntityId,
		Notify.SpawnEntityId,
		Notify.WeaponId,
		*Notify.Origin.ToCompactString(),
		*Notify.Direction.ToCompactString());
	
	const uint32 GrenadeEntityId = Notify.SpawnEntityId;
	const EItemID ItemID = static_cast<EItemID>(Notify.WeaponId);

	if (GrenadeEntityId == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoteThrowGrenade failed: invalid grenade entity id"));
		return;
	}

	SpawnThrowableProjectileFromNetwork(
		GrenadeEntityId,
		ItemID,
		Notify.Origin,
		Notify.Direction);
}

ATimeThiefThrowableProjectile* UTimeThiefThrowableComponent::SpawnThrowableProjectileFromNetwork(uint32 GrenadeEntityId,
                                                                                                 EItemID ItemID, const FVector& Origin, const FVector& Direction)
{
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	APawn* InstigatorPawn = Cast<APawn>(OwnerActor);

	if (!World || !ThrowableProjectileClass || !OwnerActor)
	{
		return nullptr;
	}

	const FTimeThiefThrowableDefinition Definition = ResolveThrowableDefinition(ItemID);
	const FTimeThiefThrowableThrowSettings& ThrowSettings = Definition.ThrowSettings;

	FVector ThrowDirection = Direction.GetSafeNormal();
	if (ThrowDirection.IsNearlyZero())
	{
		ThrowDirection = OwnerActor->GetActorForwardVector();
	}

	const FVector InitialVelocity =
		ThrowDirection * ThrowSettings.ThrowSpeed;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = InstigatorPawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATimeThiefThrowableProjectile* Projectile =
		World->SpawnActor<ATimeThiefThrowableProjectile>(
			ThrowableProjectileClass,
			FTransform(InitialVelocity.Rotation(), Origin),
			SpawnParams);

	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->InitializeThrowable(ItemID, OwnerActor, InstigatorPawn, Definition.ProjectileSettings);
	Projectile->SetThrowableMesh();
	
	bool bIsLocalOwner = false;
	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		auto* PlayerPawn = Cast<ATimeThiefPlayerCharacter>(InstigatorPawn);
		if (PlayerPawn)
		{
			uint32 PlayerPawnEntityId = PlayerPawn->GetEntityId();
			
			bIsLocalOwner = NGIS->IsLocalPlayerEntity(PlayerPawnEntityId);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] Could not determine local ownership for grenade. InstigatorPawn=%s"),
				*GetNameSafe(InstigatorPawn));
		}
	}
	
	if (bIsLocalOwner)
	{
		Projectile->InitializeNetworkSyncAsLocalOwner(GrenadeEntityId);
		Projectile->LaunchThrowable(InitialVelocity, ThrowSettings.FuseTime);
	}
	else
	{
		Projectile->InitializeNetworkSyncAsRemoteProxy(GrenadeEntityId);

		// RemoteProxy는 ProjectileMovement로 직접 날리면 안 됨.
		// 이후 GrenadeMoveSync 패킷으로 보간만 함.
	}

	if (auto* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		FEntityRuntimeEntry EntityEntry;
		EntityEntry.EntityId = GrenadeEntityId;
		EntityEntry.Actor = Projectile;
		FNetworkEntityState& EntityState = EntityEntry.State;
		EntityState.EntityId = GrenadeEntityId;
	
		EntityState.ObjectType = se::common::OBJ_PROJECTILE;
		EntityState.TemplateId = static_cast<uint32>(ItemID);

		NGIS->NetworkEntryAdd(GrenadeEntityId, EntityEntry);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] Failed to register network grenade: NetworkGameInstanceSubsystem not found."));
	}

	if (ATimeThiefPlayerCharacter* PlayerCharacter = Cast<ATimeThiefPlayerCharacter>(OwnerActor))
	{
		PlayThrowAnimation(PlayerCharacter, ThrowSettings);
		PlayThrowSound(ThrowSettings, Origin);
	}

	return Projectile;
}

bool UTimeThiefThrowableComponent::CanThrowItem(EItemID ItemID) const
{
	const ATimeThiefPlayerCharacter* PlayerCharacter = GetPlayerCharacter();
	const UInventorySystemComponent* InventoryComponent = GetInventoryComponent();
	if (!PlayerCharacter || !InventoryComponent)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] CanThrow false: PlayerCharacter=%s Inventory=%s"),
			*GetNameSafe(PlayerCharacter),
			*GetNameSafe(InventoryComponent));
#endif
		return false;
	}

	if (PlayerCharacter->bIsDead)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] CanThrow false: player is dead."));
#endif
		return false;
	}

	if (!IsSupportedThrowableItem(ItemID))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] CanThrow false: unsupported or no equipped throwable. ItemID=%d Grenade=%d Smoke=%d SIZE=%d"),
			static_cast<int32>(ItemID),
			static_cast<int32>(EItemID::Grenade),
			static_cast<int32>(EItemID::SmokeGrenade),
			static_cast<int32>(EItemID::SIZE));
#endif
		return false;
	}

	// Temporarily disabled for local throw testing without inventory counts.
	// if (InventoryComponent->GetItemQuantity(ItemID) <= 0)
	// {
	// 	return false;
	// }

	const UWorld* World = GetWorld();
	const bool bCooldownReady = !World || World->GetTimeSeconds() + KINDA_SMALL_NUMBER >= NextAllowedThrowTime;
#if !UE_BUILD_SHIPPING
	if (!bCooldownReady)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] CanThrow false: cooldown. Now=%.3f NextAllowed=%.3f"),
			World->GetTimeSeconds(),
			NextAllowedThrowTime);
	}
#endif
	return bCooldownReady;
}

bool UTimeThiefThrowableComponent::IsSupportedThrowableItem(EItemID ItemID) const
{
	return ItemID == EItemID::Grenade || ItemID == EItemID::SmokeGrenade;
}

EItemID UTimeThiefThrowableComponent::GetEquippedThrowableItem() const
{
	if (const UInventorySystemComponent* InventoryComponent = GetInventoryComponent())
	{
		return InventoryComponent->GetThrowableEquipment();
	}

	return EItemID::SIZE;
}

FTimeThiefThrowableDefinition UTimeThiefThrowableComponent::ResolveThrowableDefinition(EItemID ItemID) const
{
	if (ThrowableData)
	{
		return ThrowableData->GetDefinitionOrDefault(ItemID);
	}

	return UTimeThiefThrowableData::MakeDefaultDefinition(ItemID);
}

FVector UTimeThiefThrowableComponent::ResolveThrowOrigin(const ATimeThiefPlayerCharacter* PlayerCharacter) const
{
	if (!PlayerCharacter)
	{
		return FVector::ZeroVector;
	}

	if (USkeletalMeshComponent* AttachMesh = PlayerCharacter->GetWeaponAttachMesh())
	{
		if (AttachMesh->DoesSocketExist(ThrowSocketName))
		{
			return AttachMesh->GetSocketLocation(ThrowSocketName);
		}
	}

	const FVector Forward = PlayerCharacter->GetActorForwardVector();
	return PlayerCharacter->GetPawnViewLocation() + (Forward * 60.0f) + (FVector::UpVector * 20.0f);
}

FVector UTimeThiefThrowableComponent::ResolveThrowVelocity(const ATimeThiefPlayerCharacter* PlayerCharacter, const FVector& ThrowOrigin, const FTimeThiefThrowableThrowSettings& ThrowSettings) const
{
	if (!PlayerCharacter)
	{
		return FVector::ZeroVector;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = PlayerCharacter->GetActorForwardVector();
	if (!UTimeThiefAimStatics::ResolveAimView(PlayerCharacter, ViewLocation, ViewDirection))
	{
		ViewLocation = PlayerCharacter->GetPawnViewLocation();
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<ATimeThiefPlayerCharacter*>(PlayerCharacter));

	FHitResult HitResult;
	FVector TraceEnd = FVector::ZeroVector;
	UTimeThiefAimStatics::TraceFromView(
		GetWorld(),
		ViewLocation,
		ViewDirection,
		AimTraceRange,
		ActorsToIgnore,
		HitResult,
		TraceEnd,
		ECC_Visibility,
		true,
		false);

	const FVector TargetLocation = HitResult.bBlockingHit ? HitResult.ImpactPoint : TraceEnd;
	const FVector ThrowDirection = UTimeThiefAimStatics::ResolveAimDirectionToTarget(ThrowOrigin, TargetLocation, ViewDirection);
	return (ThrowDirection * ThrowSettings.ThrowSpeed) + (FVector::UpVector * ThrowSettings.ThrowUpwardVelocity);
}

uint32 UTimeThiefThrowableComponent::ResolveGrenadeTypeForFutureNetwork(EItemID ItemID) const
{
	switch (ItemID)
	{
	case EItemID::Grenade:
		return 1;
	case EItemID::SmokeGrenade:
		return 2;
	default:
		return 0;
	}
}

void UTimeThiefThrowableComponent::PlayThrowAnimation(ATimeThiefPlayerCharacter* PlayerCharacter, const FTimeThiefThrowableThrowSettings& ThrowSettings) const
{
	if (!PlayerCharacter || !ThrowSettings.ThrowAnimation)
	{
#if !UE_BUILD_SHIPPING
		if (!ThrowSettings.ThrowAnimation)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ThrowableDebug][Component] Throw animation skipped: ThrowAnimation is not assigned."));
		}
#endif
		return;
	}

	PlayerCharacter->PlayAnimationOnAllMeshes(ThrowSettings.ThrowAnimation, ThrowSettings.ThrowAnimSlot);
}

void UTimeThiefThrowableComponent::PlayThrowSound(const FTimeThiefThrowableThrowSettings& ThrowSettings, const FVector& ThrowOrigin) const
{
	if (!ThrowSettings.ThrowSound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(this, ThrowSettings.ThrowSound, ThrowOrigin);
}

ATimeThiefPlayerCharacter* UTimeThiefThrowableComponent::GetPlayerCharacter() const
{
	return Cast<ATimeThiefPlayerCharacter>(GetOwner());
}

UInventorySystemComponent* UTimeThiefThrowableComponent::GetInventoryComponent() const
{
	if (const ATimeThiefPlayerCharacter* PlayerCharacter = GetPlayerCharacter())
	{
		return PlayerCharacter->GetInventoryComponent();
	}

	return nullptr;
}
