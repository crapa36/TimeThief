#include "Components/Skill/TimeThiefSkillComponent.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefSkillDummyCharacter.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/TimeThiefHealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimeThiefGameplayTags.h"
#include "Utils/TimeThiefAimStatics.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"

namespace
{
	constexpr uint32 EnhanceSkillId = 1;
	constexpr uint32 DummySkillId = 2;
	constexpr uint32 RewindSkillId = 3;
	constexpr uint32 RewindRequestDurationMs = 3000;

	constexpr uint32 Slot1Index = 0;
	constexpr uint32 Slot2Index = 1;

	float PercentToMultiplier(uint32 Percent)
	{
		return 1.0f + static_cast<float>(Percent) / 100.0f;
	}

	FVector ResolvePlanarDirection(const FVector& Direction, const FVector& FallbackDirection)
	{
		return UTimeThiefAimStatics::NormalizeAimDirection(
			FVector(Direction.X, Direction.Y, 0.0f),
			FVector(FallbackDirection.X, FallbackDirection.Y, 0.0f));
	}
}

UTimeThiefSkillComponent::UTimeThiefSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	DummyClass = ATimeThiefSkillDummyCharacter::StaticClass();
}

void UTimeThiefSkillComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTimeThiefSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnhanceTimerHandle);
	}

	StopEnhanceEffects();
	Super::EndPlay(EndPlayReason);
}

void UTimeThiefSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bRewinding)
	{
		TickRewind(DeltaTime);
		return;
	}

	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		return;
	}

	SnapshotAccumulatorSeconds += DeltaTime;
	if (SnapshotAccumulatorSeconds >= Tuning.RewindSnapshotIntervalSeconds)
	{
		SnapshotAccumulatorSeconds = 0.0f;
		RecordRewindSnapshot();
	}
}

bool UTimeThiefSkillComponent::HandleSkillInput(FGameplayTag InputTag)
{
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (InputTag == Tags.InputTag_Skill_Slot1)
	{
		return TryUseSkillSlot(Slot1Index);
	}
	if (InputTag == Tags.InputTag_Skill_Slot2)
	{
		return TryUseSkillSlot(Slot2Index);
	}

	return false;
}

void UTimeThiefSkillComponent::ApplySkillSnapshot(const TArray<FTimeThiefSkillSlotState>& InEquippedSlots)
{
	SkillSlots.Reset(InEquippedSlots.Num());

	for (FTimeThiefSkillSlotState Slot : InEquippedSlots)
	{
		Slot.SkillType = ResolveSkillTypeFromId(static_cast<uint32>(Slot.SkillId));
		SkillSlots.Add(Slot);
	}
}

bool UTimeThiefSkillComponent::SetEquippedSkillSlot(uint32 SlotIndex, uint32 SkillId)
{
	const ETimeThiefSkillType SkillType = ResolveSkillTypeFromId(SkillId);

	for (int32 Index = SkillSlots.Num() - 1; Index >= 0; --Index)
	{
		if (SkillSlots[Index].SlotIndex == static_cast<int32>(SlotIndex))
		{
			SkillSlots.RemoveAt(Index);
		}
	}

	FTimeThiefSkillSlotState SlotState;
	SlotState.SlotIndex = static_cast<int32>(SlotIndex);
	SlotState.SkillId = static_cast<int32>(SkillId);
	SlotState.SkillType = SkillType;
	SkillSlots.Add(SlotState);
	SkillSlots.Sort([](const FTimeThiefSkillSlotState& Lhs, const FTimeThiefSkillSlotState& Rhs)
	{
		return Lhs.SlotIndex < Rhs.SlotIndex;
	});
	return true;
}

bool UTimeThiefSkillComponent::FindEquippedSkillSlot(uint32 SkillId, uint32& OutSlotIndex) const
{
	for (const FTimeThiefSkillSlotState& Slot : SkillSlots)
	{
		if (Slot.SkillId == static_cast<int32>(SkillId))
		{
			OutSlotIndex = static_cast<uint32>(Slot.SlotIndex);
			return true;
		}
	}

	return false;
}

bool UTimeThiefSkillComponent::FindFirstAvailableSkillSlot(uint32& OutSlotIndex) const
{
	for (uint32 SlotIndex = Slot1Index; SlotIndex <= Slot2Index; ++SlotIndex)
	{
		const FTimeThiefSkillSlotState* Slot = FindSlot(SlotIndex);
		if (Slot == nullptr || Slot->SkillType == ETimeThiefSkillType::None || Slot->SkillId == 0)
		{
			OutSlotIndex = SlotIndex;
			return true;
		}
	}

	return false;
}

void UTimeThiefSkillComponent::ApplyTimeAccelEffect(uint32 DurationMs, uint32 FireRateBonusPercent, uint32 MoveSpeedBonusPercent)
{
	const float DurationSeconds = DurationMs > 0
		? static_cast<float>(DurationMs) / 1000.0f
		: Tuning.EnhanceDurationSeconds;

	StartEnhance(
		DurationSeconds,
		PercentToMultiplier(MoveSpeedBonusPercent),
		PercentToMultiplier(FireRateBonusPercent),
		PercentToMultiplier(FireRateBonusPercent),
		Tuning.EnhanceDamageMultiplier,
		Tuning.EnhanceEquipSpeedMultiplier);
}

void UTimeThiefSkillComponent::SpawnDummyEffect(const FVector& StartPosition, const FVector& Direction, float MoveSpeed, uint32 DurationMs)
{
	UWorld* World = GetWorld();
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!World || !OwnerCharacter)
	{
		return;
	}

	const FVector MoveDirection = ResolvePlanarDirection(Direction, OwnerCharacter->GetActorForwardVector());
	const FRotator SpawnRotation = MoveDirection.IsNearlyZero()
		? OwnerCharacter->GetActorRotation()
		: MoveDirection.Rotation();

	TSubclassOf<ATimeThiefSkillDummyCharacter> ClassToSpawn = DummyClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = ATimeThiefSkillDummyCharacter::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = Cast<APawn>(OwnerCharacter);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATimeThiefSkillDummyCharacter* Dummy = World->SpawnActor<ATimeThiefSkillDummyCharacter>(
		ClassToSpawn,
		StartPosition,
		SpawnRotation,
		SpawnParams);

	if (!Dummy)
	{
		return;
	}

	const float LifetimeSeconds = DurationMs > 0
		? static_cast<float>(DurationMs) / 1000.0f
		: Tuning.DummyLifetimeSeconds;

	float ResolvedMoveSpeed = MoveSpeed;
	if (ResolvedMoveSpeed <= 0.0f)
	{
		if (const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			ResolvedMoveSpeed = Movement->MaxWalkSpeed;
		}
	}

	Dummy->InitializeFromSource(OwnerCharacter, MoveDirection, ResolvedMoveSpeed, LifetimeSeconds);
}

void UTimeThiefSkillComponent::ApplyRewindEffect(uint32 DurationMs, uint32 RewindDurationMs, uint32 InvulnerableDurationMs, int32 TargetHealth, const FVector& TargetPosition, bool bHasTargetPosition)
{
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || bRewinding)
	{
		return;
	}

	ActiveRewindPath.Reset();
	RewindElapsedSeconds = 0.0f;
	RewindPlaybackSeconds = DurationMs > 0
		? static_cast<float>(DurationMs) / 1000.0f
		: Tuning.RewindPlaybackSeconds;

	if (bHasTargetPosition)
	{
		FTimeThiefRewindSnapshot CurrentSnapshot;
		CurrentSnapshot.Location = OwnerCharacter->GetActorLocation();
		CurrentSnapshot.Rotation = OwnerCharacter->GetActorRotation();

		FTimeThiefRewindSnapshot TargetSnapshot = CurrentSnapshot;
		TargetSnapshot.Location = TargetPosition;
		TargetSnapshot.Health = TargetHealth;

		ActiveRewindPath.Add(CurrentSnapshot);
		ActiveRewindPath.Add(TargetSnapshot);
	}
	else
	{
		const float TargetSecondsAgo = RewindDurationMs > 0
			? static_cast<float>(RewindDurationMs) / 1000.0f
			: Tuning.RewindHistorySeconds;
		if (!BuildRewindPathFromHistory(TargetSecondsAgo))
		{
			return;
		}
	}

	bRewinding = true;
	SnapshotAccumulatorSeconds = 0.0f;

	if (UTimeThiefPlayerCombatComponent* CombatComponent = OwnerCharacter->FindComponentByClass<UTimeThiefPlayerCombatComponent>())
	{
		CombatComponent->ForceStopCombatInput();
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		SavedMovementMode = Movement->MovementMode;
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (RewindTrailEffect)
	{
		ActiveRewindTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(
			RewindTrailEffect,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true);
	}

	(void)InvulnerableDurationMs;
}

float UTimeThiefSkillComponent::GetMoveSpeedMultiplier() const
{
	return bEnhanceActive ? FMath::Max(ActiveMoveSpeedMultiplier, 0.01f) : 1.0f;
}

float UTimeThiefSkillComponent::GetFireRateMultiplier() const
{
	return bEnhanceActive ? FMath::Max(ActiveFireRateMultiplier, 0.01f) : 1.0f;
}

float UTimeThiefSkillComponent::GetReloadSpeedMultiplier() const
{
	return bEnhanceActive ? FMath::Max(ActiveReloadSpeedMultiplier, 0.01f) : 1.0f;
}

float UTimeThiefSkillComponent::GetDamageMultiplier() const
{
	return bEnhanceActive ? FMath::Max(ActiveDamageMultiplier, 0.01f) : 1.0f;
}

float UTimeThiefSkillComponent::GetEquipSpeedMultiplier() const
{
	return bEnhanceActive ? FMath::Max(ActiveEquipSpeedMultiplier, 0.01f) : 1.0f;
}

ETimeThiefSkillType UTimeThiefSkillComponent::ResolveSkillTypeFromId(uint32 SkillId)
{
	switch (SkillId)
	{
	case EnhanceSkillId:
		return ETimeThiefSkillType::Enhance;
	case DummySkillId:
		return ETimeThiefSkillType::Dummy;
	case RewindSkillId:
		return ETimeThiefSkillType::Rewind;
	default:
		return ETimeThiefSkillType::None;
	}
}

uint32 UTimeThiefSkillComponent::ResolveSkillIdFromType(ETimeThiefSkillType SkillType)
{
	switch (SkillType)
	{
	case ETimeThiefSkillType::Enhance:
		return EnhanceSkillId;
	case ETimeThiefSkillType::Dummy:
		return DummySkillId;
	case ETimeThiefSkillType::Rewind:
		return RewindSkillId;
	default:
		return 0;
	}
}

bool UTimeThiefSkillComponent::TryUseSkillSlot(uint32 SlotIndex)
{
	const FTimeThiefSkillSlotState* Slot = FindSlot(SlotIndex);
	if (!Slot)
	{
		return TryUseSkill(ETimeThiefSkillType::None, SlotIndex, 0);
	}

	return TryUseSkill(Slot->SkillType, static_cast<uint32>(Slot->SlotIndex), static_cast<uint32>(Slot->SkillId));
}

bool UTimeThiefSkillComponent::TryUseSkill(ETimeThiefSkillType SkillType, uint32 SlotIndex, uint32 SkillId)
{
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		return false;
	}

	FUseSkillRequestDetail Detail;
	switch (SkillType)
	{
	case ETimeThiefSkillType::Dummy:
		{
			FVector ViewLocation = FVector::ZeroVector;
			FVector ViewDirection = OwnerCharacter->GetActorForwardVector();
			UTimeThiefAimStatics::ResolveAimView(OwnerCharacter, ViewLocation, ViewDirection);

			const FVector Direction = ResolvePlanarDirection(ViewDirection, OwnerCharacter->GetActorForwardVector());
			const FVector StartPosition = OwnerCharacter->GetActorLocation();
			Detail = FUseSkillRequestDetail::MakeAfterImage(StartPosition, Direction);
			break;
		}
	case ETimeThiefSkillType::Rewind:
		{
			const float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			const float TargetTime = WorldTime - static_cast<float>(RewindRequestDurationMs) / 1000.0f;

			FVector TargetPosition = OwnerCharacter->GetActorLocation();
			if (RewindSnapshots.Num() > 0)
			{
				TargetPosition = RewindSnapshots[0].Location;
				for (int32 Index = 1; Index < RewindSnapshots.Num(); ++Index)
				{
					const FTimeThiefRewindSnapshot& PreviousSnapshot = RewindSnapshots[Index - 1];
					const FTimeThiefRewindSnapshot& NextSnapshot = RewindSnapshots[Index];
					if (NextSnapshot.TimeSeconds >= TargetTime)
					{
						const float TimeRange = NextSnapshot.TimeSeconds - PreviousSnapshot.TimeSeconds;
						const float Alpha = TimeRange > KINDA_SMALL_NUMBER
							? FMath::Clamp((TargetTime - PreviousSnapshot.TimeSeconds) / TimeRange, 0.0f, 1.0f)
							: 0.0f;
						TargetPosition = FMath::Lerp(PreviousSnapshot.Location, NextSnapshot.Location, Alpha);
						break;
					}

					TargetPosition = NextSnapshot.Location;
				}
			}

			Detail = FUseSkillRequestDetail::MakeRewind(RewindRequestDurationMs, TargetPosition);
			break;
		}
	default:
		break;
	}

	if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
	{
		if (NGIS->CanSendGameplayPacket())
		{
			NGIS->SendUseSkill(SlotIndex, SkillId, Detail);
			return true;
		}
	}

	switch (SkillType)
	{
	case ETimeThiefSkillType::Enhance:
		StartEnhance(
			Tuning.EnhanceDurationSeconds,
			Tuning.EnhanceMoveSpeedMultiplier,
			Tuning.EnhanceFireRateMultiplier,
			Tuning.EnhanceReloadSpeedMultiplier,
			Tuning.EnhanceDamageMultiplier,
			Tuning.EnhanceEquipSpeedMultiplier);
		break;
	case ETimeThiefSkillType::Dummy:
		SpawnDummyEffect(Detail.StartPosition, Detail.Direction, 0.0f, static_cast<uint32>(Tuning.DummyLifetimeSeconds * 1000.0f));
		break;
	case ETimeThiefSkillType::Rewind:
		ApplyRewindEffect(
			static_cast<uint32>(Tuning.RewindPlaybackSeconds * 1000.0f),
			static_cast<uint32>(Tuning.RewindHistorySeconds * 1000.0f),
			0,
			0,
			FVector::ZeroVector,
			false);
		break;
	default:
		break;
	}

	return true;
}

const FTimeThiefSkillSlotState* UTimeThiefSkillComponent::FindSlot(uint32 SlotIndex) const
{
	for (const FTimeThiefSkillSlotState& Slot : SkillSlots)
	{
		if (Slot.SlotIndex == static_cast<int32>(SlotIndex))
		{
			return &Slot;
		}
	}

	return nullptr;
}

FTimeThiefSkillSlotState* UTimeThiefSkillComponent::FindSlot(uint32 SlotIndex)
{
	for (FTimeThiefSkillSlotState& Slot : SkillSlots)
	{
		if (Slot.SlotIndex == static_cast<int32>(SlotIndex))
		{
			return &Slot;
		}
	}

	return nullptr;
}

void UTimeThiefSkillComponent::RecordRewindSnapshot()
{
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		return;
	}

	FTimeThiefRewindSnapshot Snapshot;
	Snapshot.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Snapshot.Location = OwnerCharacter->GetActorLocation();
	Snapshot.Rotation = OwnerCharacter->GetActorRotation();

	if (const UTimeThiefHealthComponent* Health = OwnerCharacter->GetHealthComponent())
	{
		Snapshot.Health = Health->GetCurrentHealth();
	}

	if (const UTimeThiefPawnCombatComponent* CombatComponent = OwnerCharacter->FindComponentByClass<UTimeThiefPawnCombatComponent>())
	{
		Snapshot.ActiveWeaponTag = CombatComponent->CurrentEquippedWeaponTag;
		if (const UTimeThiefWeaponComponentBase* Weapon = CombatComponent->GetCharacterCurrentEquippedWeapon())
		{
			Snapshot.CurrentAmmo = Weapon->GetCurrentAmmo();
		}
	}

	RewindSnapshots.Add(Snapshot);

	const int32 MaxSnapshotCount = FMath::Max(2, FMath::CeilToInt(Tuning.RewindHistorySeconds / Tuning.RewindSnapshotIntervalSeconds) + 1);
	while (RewindSnapshots.Num() > MaxSnapshotCount)
	{
		RewindSnapshots.RemoveAt(0);
	}
}

void UTimeThiefSkillComponent::TickRewind(float DeltaTime)
{
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || ActiveRewindPath.Num() < 2)
	{
		FinishRewind();
		return;
	}

	RewindElapsedSeconds += DeltaTime;
	const float Alpha = RewindPlaybackSeconds > KINDA_SMALL_NUMBER
		? FMath::Clamp(RewindElapsedSeconds / RewindPlaybackSeconds, 0.0f, 1.0f)
		: 1.0f;

	const float PathPosition = Alpha * static_cast<float>(ActiveRewindPath.Num() - 1);
	const int32 SegmentIndex = FMath::Clamp(FMath::FloorToInt(PathPosition), 0, ActiveRewindPath.Num() - 2);
	const float SegmentAlpha = PathPosition - static_cast<float>(SegmentIndex);

	const FTimeThiefRewindSnapshot& From = ActiveRewindPath[SegmentIndex];
	const FTimeThiefRewindSnapshot& To = ActiveRewindPath[SegmentIndex + 1];
	const FVector Location = FMath::Lerp(From.Location, To.Location, SegmentAlpha);
	const FRotator Rotation = FQuat::Slerp(From.Rotation.Quaternion(), To.Rotation.Quaternion(), SegmentAlpha).Rotator();

	OwnerCharacter->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		bRewinding = false;
		ApplyFinalRewindSnapshot(ActiveRewindPath.Last());
		FinishRewind();
	}
}

bool UTimeThiefSkillComponent::BuildRewindPathFromHistory(float TargetSecondsAgo)
{
	if (RewindSnapshots.Num() < 2)
	{
		return false;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : RewindSnapshots.Last().TimeSeconds;
	const float TargetTime = Now - FMath::Max(TargetSecondsAgo, 0.0f);

	int32 OldestUsableIndex = 0;
	for (int32 Index = 0; Index < RewindSnapshots.Num(); ++Index)
	{
		if (RewindSnapshots[Index].TimeSeconds <= TargetTime)
		{
			OldestUsableIndex = Index;
			break;
		}
	}

	ActiveRewindPath.Reset();
	for (int32 Index = RewindSnapshots.Num() - 1; Index >= OldestUsableIndex; --Index)
	{
		ActiveRewindPath.Add(RewindSnapshots[Index]);
	}

	return ActiveRewindPath.Num() >= 2;
}

void UTimeThiefSkillComponent::FinishRewind()
{
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (OwnerCharacter)
	{
		if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			Movement->SetMovementMode(SavedMovementMode);
		}
	}

	if (ActiveRewindTrail)
	{
		ActiveRewindTrail->Deactivate();
		ActiveRewindTrail->DestroyComponent();
		ActiveRewindTrail = nullptr;
	}

	bRewinding = false;
	ActiveRewindPath.Reset();
	RewindSnapshots.Reset();
	RecordRewindSnapshot();
}

void UTimeThiefSkillComponent::ApplyFinalRewindSnapshot(const FTimeThiefRewindSnapshot& Snapshot)
{
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		return;
	}

	if (UTimeThiefHealthComponent* Health = OwnerCharacter->GetHealthComponent())
	{
		Health->SetHealth(Health->GetMaxHealth(), Snapshot.Health);
	}

	if (UTimeThiefPawnCombatComponent* CombatComponent = OwnerCharacter->FindComponentByClass<UTimeThiefPawnCombatComponent>())
	{
		if (Snapshot.ActiveWeaponTag.IsValid())
		{
			CombatComponent->EquipWeapon(Snapshot.ActiveWeaponTag);
		}

		if (UTimeThiefWeaponComponentBase* Weapon = CombatComponent->GetCharacterCurrentEquippedWeapon())
		{
			Weapon->SetAmmoFromSkillRewind(Snapshot.CurrentAmmo);
		}
	}
}

void UTimeThiefSkillComponent::StartEnhance(float DurationSeconds, float MoveSpeedMultiplier, float FireRateMultiplier, float ReloadSpeedMultiplier, float DamageMultiplier, float EquipSpeedMultiplier)
{
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		return;
	}

	bEnhanceActive = true;
	ActiveMoveSpeedMultiplier = MoveSpeedMultiplier;
	ActiveFireRateMultiplier = FireRateMultiplier;
	ActiveReloadSpeedMultiplier = ReloadSpeedMultiplier;
	ActiveDamageMultiplier = DamageMultiplier;
	ActiveEquipSpeedMultiplier = EquipSpeedMultiplier;

	RefreshSkillModifiedStats();

	StopEnhanceEffects();
	if (EnhanceAuraEffect)
	{
		ActiveEnhanceAura = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EnhanceAuraEffect,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true);
	}
	if (EnhanceLightningEffect)
	{
		ActiveEnhanceLightning = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EnhanceLightningEffect,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnhanceTimerHandle);
		World->GetTimerManager().SetTimer(EnhanceTimerHandle, this, &UTimeThiefSkillComponent::StopEnhance, FMath::Max(DurationSeconds, 0.01f), false);
	}
}

void UTimeThiefSkillComponent::StopEnhance()
{
	bEnhanceActive = false;
	ActiveMoveSpeedMultiplier = 1.0f;
	ActiveFireRateMultiplier = 1.0f;
	ActiveReloadSpeedMultiplier = 1.0f;
	ActiveDamageMultiplier = 1.0f;
	ActiveEquipSpeedMultiplier = 1.0f;

	StopEnhanceEffects();
	RefreshSkillModifiedStats();
}

void UTimeThiefSkillComponent::RefreshSkillModifiedStats() const
{
	if (const ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter())
	{
		if (UTimeThiefPlayerCombatComponent* CombatComponent = OwnerCharacter->FindComponentByClass<UTimeThiefPlayerCombatComponent>())
		{
			CombatComponent->RefreshMoveSpeed();
		}
	}
}

void UTimeThiefSkillComponent::StopEnhanceEffects()
{
	if (ActiveEnhanceAura)
	{
		ActiveEnhanceAura->Deactivate();
		ActiveEnhanceAura->DestroyComponent();
		ActiveEnhanceAura = nullptr;
	}

	if (ActiveEnhanceLightning)
	{
		ActiveEnhanceLightning->Deactivate();
		ActiveEnhanceLightning->DestroyComponent();
		ActiveEnhanceLightning = nullptr;
	}
}

ATimeThiefCharacterBase* UTimeThiefSkillComponent::GetOwnerCharacter() const
{
	return Cast<ATimeThiefCharacterBase>(GetOwner());
}
