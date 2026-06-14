#include "Components/Skill/TimeThiefSkillComponent.h"

#include "Character/TimeThiefCharacterBase.h"
#include "Character/TimeThiefSkillDummyCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/Combat/TimeThiefPlayerCombatComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimeThiefHealthComponent.h"
#include "Components/Wire/TimeThiefWireComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimeThiefGameplayTags.h"
#include "Utils/TimeThiefAimStatics.h"
#include "Weapon/Components/TimeThiefWeaponComponentBase.h"
#include "MorphingMesh/Core/LiquidMeshComponent.h"
#include "MorphingMesh/MorphingMeshComponent.h"
#include "Engine/BlendableInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr uint32 EnhanceSkillId = 1;
	constexpr uint32 DummySkillId = 2;
	constexpr uint32 RewindSkillId = 3;
	constexpr uint32 RewindRequestDurationMs = 3000;

	constexpr uint32 Slot1Index = 0;
	constexpr uint32 Slot2Index = 1;
	const FName EffectStrengthParameterName(TEXT("EffectStrength"));

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

	FVector GetMeshCenterRelativeLocation(const USkeletalMeshComponent& MeshComponent)
	{
		return MeshComponent.GetComponentTransform().InverseTransformPosition(MeshComponent.Bounds.Origin);
	}

	FVector GetMeshCenterWorldLocation(const ACharacter& Character)
	{
		if (const USkeletalMeshComponent* MeshComponent = Character.GetMesh())
		{
			return MeshComponent->Bounds.Origin;
		}

		return Character.GetActorLocation();
	}
}

UTimeThiefSkillComponent::UTimeThiefSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	DummyClass = ATimeThiefSkillDummyCharacter::StaticClass();
}

void UTimeThiefSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnhanceTimerHandle);
	}

	StopEnhanceVFX(true);
	StopRewindVFX();
	RestoreRewindMeshes();

	Super::EndPlay(EndPlayReason);
}

void UTimeThiefSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateEnhanceScreenPostProcess(DeltaTime);
	TickSkillCooldowns(DeltaTime);

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

	for (int32 Index = SkillCooldowns.Num() - 1; Index >= 0; --Index)
	{
		const FTimeThiefSkillSlotState* Slot = FindSlot(static_cast<uint32>(SkillCooldowns[Index].SlotIndex));
		if (!Slot || Slot->SkillId != SkillCooldowns[Index].SkillId)
		{
			const uint32 SlotIndex = static_cast<uint32>(SkillCooldowns[Index].SlotIndex);
			SkillCooldowns.RemoveAt(Index);
			OnSkillCooldownChanged.Broadcast(SlotIndex);
		}
	}

	OnSkillSlotsChanged.Broadcast();
}

void UTimeThiefSkillComponent::SetEquippedSkillSlot(uint32 SlotIndex, uint32 SkillId)
{
	const FTimeThiefSkillSlotState* PreviousSlot = FindSlot(SlotIndex);
	const bool bSkillChanged = PreviousSlot == nullptr || PreviousSlot->SkillId != static_cast<int32>(SkillId);
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

	if (bSkillChanged)
	{
		ResetSkillCooldown(SlotIndex, SkillId);
	}

	OnSkillSlotsChanged.Broadcast();
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

void UTimeThiefSkillComponent::ApplySkillCooldown(uint32 SlotIndex, uint32 SkillId, uint32 RemainingCooldownMs)
{
	const float RemainingSeconds = static_cast<float>(RemainingCooldownMs) / 1000.0f;
	FTimeThiefSkillCooldownState* CooldownState = FindCooldownState(SlotIndex);
	if (!CooldownState)
	{
		CooldownState = &SkillCooldowns.AddDefaulted_GetRef();
		CooldownState->SlotIndex = static_cast<int32>(SlotIndex);
	}

	CooldownState->SkillId = static_cast<int32>(SkillId);
	CooldownState->RemainingSeconds = FMath::Max(RemainingSeconds, 0.0f);
	CooldownState->bCoolingDown = CooldownState->RemainingSeconds > 0.0f;

	if (!CooldownState->bCoolingDown)
	{
		CooldownState->TotalSeconds = 0.0f;
		OnSkillCooldownChanged.Broadcast(SlotIndex);
		return;
	}

	if (CooldownState->TotalSeconds <= 0.0f || CooldownState->RemainingSeconds > CooldownState->TotalSeconds)
	{
		CooldownState->TotalSeconds = CooldownState->RemainingSeconds;
	}

	OnSkillCooldownChanged.Broadcast(SlotIndex);
}

bool UTimeThiefSkillComponent::GetSkillSlotState(int32 SlotIndex, FTimeThiefSkillSlotState& OutSlotState) const
{
	if (SlotIndex < 0)
	{
		return false;
	}

	if (const FTimeThiefSkillSlotState* Slot = FindSlot(static_cast<uint32>(SlotIndex)))
	{
		OutSlotState = *Slot;
		return true;
	}

	OutSlotState = FTimeThiefSkillSlotState();
	OutSlotState.SlotIndex = SlotIndex;
	return false;
}

FTimeThiefSkillCooldownState UTimeThiefSkillComponent::GetSkillCooldownState(int32 SlotIndex) const
{
	FTimeThiefSkillCooldownState Result;
	Result.SlotIndex = SlotIndex;
	if (SlotIndex < 0)
	{
		return Result;
	}

	if (const FTimeThiefSkillCooldownState* CooldownState = FindCooldownState(static_cast<uint32>(SlotIndex)))
	{
		Result = *CooldownState;
	}

	return Result;
}

float UTimeThiefSkillComponent::GetSkillCooldownPercent(int32 SlotIndex) const
{
	if (SlotIndex < 0)
	{
		return 0.0f;
	}

	const FTimeThiefSkillCooldownState* CooldownState = FindCooldownState(static_cast<uint32>(SlotIndex));
	if (!CooldownState || CooldownState->TotalSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(CooldownState->RemainingSeconds / CooldownState->TotalSeconds, 0.0f, 1.0f);
}

float UTimeThiefSkillComponent::GetSkillCooldownRemainingSeconds(int32 SlotIndex) const
{
	if (SlotIndex < 0)
	{
		return 0.0f;
	}

	const FTimeThiefSkillCooldownState* CooldownState = FindCooldownState(static_cast<uint32>(SlotIndex));
	return CooldownState ? FMath::Max(CooldownState->RemainingSeconds, 0.0f) : 0.0f;
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
	Dummy->SetDespawnNiagaraEffect(DummyDespawnNiagaraEffect);
	PlaySkillNiagaraAtLocation(DummySpawnNiagaraEffect, GetMeshCenterWorldLocation(*Dummy), SpawnRotation);
}

void UTimeThiefSkillComponent::ApplyRewindEffect(uint32 DurationMs, uint32 RewindDurationMs, int32 TargetHealth, const FVector& TargetPosition, bool bHasTargetPosition)
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

	const float TargetSecondsAgo = RewindDurationMs > 0
		? static_cast<float>(RewindDurationMs) / 1000.0f
		: Tuning.RewindHistorySeconds;
	const bool bHasHistoryPath = BuildRewindPathFromHistory(TargetSecondsAgo);

	if (bHasTargetPosition)
	{
		if (bHasHistoryPath)
		{
			FTimeThiefRewindSnapshot& TargetSnapshot = ActiveRewindPath.Last();
			TargetSnapshot.Location = TargetPosition;
			TargetSnapshot.Health = TargetHealth;
		}
		else
		{
			FTimeThiefRewindSnapshot CurrentSnapshot = CaptureRewindSnapshot();
			FTimeThiefRewindSnapshot TargetSnapshot = CurrentSnapshot;
			TargetSnapshot.Location = TargetPosition;
			TargetSnapshot.Health = TargetHealth;

			ActiveRewindPath.Add(CurrentSnapshot);
			ActiveRewindPath.Add(TargetSnapshot);
		}
	}
	else if (!bHasHistoryPath)
	{
		return;
	}

	bRewinding = true;
	SnapshotAccumulatorSeconds = 0.0f;
	HideRewindMeshes(*OwnerCharacter);

	if (UTimeThiefPlayerCombatComponent* CombatComponent = OwnerCharacter->FindComponentByClass<UTimeThiefPlayerCombatComponent>())
	{
		CombatComponent->ForceStopCombatInput();
	}

	if (UTimeThiefWireComponent* WireComponent = OwnerCharacter->FindComponentByClass<UTimeThiefWireComponent>())
	{
		WireComponent->ReleaseWire();
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		SavedMovementMode = Movement->MovementMode;
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	StartRewindVFX(*OwnerCharacter);
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

FTimeThiefSkillCooldownState* UTimeThiefSkillComponent::FindCooldownState(uint32 SlotIndex)
{
	for (FTimeThiefSkillCooldownState& CooldownState : SkillCooldowns)
	{
		if (CooldownState.SlotIndex == static_cast<int32>(SlotIndex))
		{
			return &CooldownState;
		}
	}

	return nullptr;
}

const FTimeThiefSkillCooldownState* UTimeThiefSkillComponent::FindCooldownState(uint32 SlotIndex) const
{
	for (const FTimeThiefSkillCooldownState& CooldownState : SkillCooldowns)
	{
		if (CooldownState.SlotIndex == static_cast<int32>(SlotIndex))
		{
			return &CooldownState;
		}
	}

	return nullptr;
}

void UTimeThiefSkillComponent::TickSkillCooldowns(float DeltaTime)
{
	for (FTimeThiefSkillCooldownState& CooldownState : SkillCooldowns)
	{
		if (!CooldownState.bCoolingDown)
		{
			continue;
		}

		CooldownState.RemainingSeconds = FMath::Max(0.0f, CooldownState.RemainingSeconds - DeltaTime);
		if (CooldownState.RemainingSeconds <= 0.0f)
		{
			CooldownState.bCoolingDown = false;
			CooldownState.TotalSeconds = 0.0f;
			OnSkillCooldownChanged.Broadcast(static_cast<uint32>(CooldownState.SlotIndex));
		}
	}
}

void UTimeThiefSkillComponent::ResetSkillCooldown(uint32 SlotIndex, uint32 SkillId)
{
	FTimeThiefSkillCooldownState* CooldownState = FindCooldownState(SlotIndex);
	if (!CooldownState)
	{
		return;
	}

	CooldownState->SkillId = static_cast<int32>(SkillId);
	CooldownState->RemainingSeconds = 0.0f;
	CooldownState->TotalSeconds = 0.0f;
	CooldownState->bCoolingDown = false;
	OnSkillCooldownChanged.Broadcast(SlotIndex);
}

FTimeThiefRewindSnapshot UTimeThiefSkillComponent::CaptureRewindSnapshot() const
{
	FTimeThiefRewindSnapshot Snapshot;
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter)
	{
		return Snapshot;
	}

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

	return Snapshot;
}

void UTimeThiefSkillComponent::RecordRewindSnapshot()
{
	if (!GetOwnerCharacter())
	{
		return;
	}

	FTimeThiefRewindSnapshot Snapshot = CaptureRewindSnapshot();
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

	float TotalPathDistance = 0.0f;
	for (int32 Index = 0; Index < ActiveRewindPath.Num() - 1; ++Index)
	{
		TotalPathDistance += FVector::Dist(ActiveRewindPath[Index].Location, ActiveRewindPath[Index + 1].Location);
	}

	int32 SegmentIndex = ActiveRewindPath.Num() - 2;
	float SegmentAlpha = 1.0f;
	if (Alpha < 1.0f)
	{
		if (TotalPathDistance > KINDA_SMALL_NUMBER)
		{
			const float TargetDistance = TotalPathDistance * Alpha;
			float TraversedDistance = 0.0f;
			for (int32 Index = 0; Index < ActiveRewindPath.Num() - 1; ++Index)
			{
				const float SegmentDistance = FVector::Dist(ActiveRewindPath[Index].Location, ActiveRewindPath[Index + 1].Location);
				if (SegmentDistance <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				if (TraversedDistance + SegmentDistance >= TargetDistance)
				{
					SegmentIndex = Index;
					SegmentAlpha = FMath::Clamp((TargetDistance - TraversedDistance) / SegmentDistance, 0.0f, 1.0f);
					break;
				}

				TraversedDistance += SegmentDistance;
			}
		}
		else
		{
			const float PathPosition = Alpha * static_cast<float>(ActiveRewindPath.Num() - 1);
			SegmentIndex = FMath::Clamp(FMath::FloorToInt(PathPosition), 0, ActiveRewindPath.Num() - 2);
			SegmentAlpha = PathPosition - static_cast<float>(SegmentIndex);
		}
	}

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
	if (RewindSnapshots.Num() < 1)
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
			continue;
		}

		break;
	}

	ActiveRewindPath.Reset();
	ActiveRewindPath.Add(CaptureRewindSnapshot());
	for (int32 Index = RewindSnapshots.Num() - 1; Index >= OldestUsableIndex; --Index)
	{
		ActiveRewindPath.Add(RewindSnapshots[Index]);
	}

	return ActiveRewindPath.Num() >= 2;
}

void UTimeThiefSkillComponent::FinishRewind()
{
	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	const bool bPlayArrivalVFX = ActiveRewindPath.Num() >= 2;
	if (OwnerCharacter)
	{
		if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			Movement->SetMovementMode(SavedMovementMode);
		}
	}

	StopRewindVFX();
	RestoreRewindMeshes();
	if (bPlayArrivalVFX && OwnerCharacter)
	{
		PlaySkillNiagaraAtLocation(RewindArrivalNiagaraEffect, GetMeshCenterWorldLocation(*OwnerCharacter), OwnerCharacter->GetActorRotation());
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
			if (UMorphingMeshComponent* MorphingComponent = OwnerCharacter->GetMorphingMeshComponent())
			{
				MorphingComponent->SetType(FTimeThiefGameplayTags::GetMorphTargetTypeByTag(Snapshot.ActiveWeaponTag));
			}
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
	StartEnhanceVFX(*OwnerCharacter);

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

	StopEnhanceVFX();
	RefreshSkillModifiedStats();
}

void UTimeThiefSkillComponent::StartEnhanceVFX(ATimeThiefCharacterBase& OwnerCharacter)
{
	StopSkillNiagaraComponent(ActiveEnhanceAuraNiagaraComponent);
	StartEnhanceScreenVFX(OwnerCharacter);
	PlaySkillSoundAtLocation(EnhanceStartSound, GetMeshCenterWorldLocation(OwnerCharacter));

	if (USkeletalMeshComponent* MeshComponent = OwnerCharacter.GetMesh())
	{
		const FVector MeshCenterRelativeLocation = GetMeshCenterRelativeLocation(*MeshComponent);
		PlaySkillNiagaraAtLocation(
			EnhanceStartNiagaraEffect,
			MeshComponent->Bounds.Origin,
			OwnerCharacter.GetActorRotation());

		ActiveEnhanceAuraNiagaraComponent = PlaySkillNiagaraAttached(
			EnhanceAuraNiagaraEffect,
			*MeshComponent,
			MeshCenterRelativeLocation);
	}
}

void UTimeThiefSkillComponent::StopEnhanceVFX(bool bImmediate)
{
	StopSkillNiagaraComponent(ActiveEnhanceAuraNiagaraComponent);
	StopEnhanceScreenVFX(bImmediate);
}

void UTimeThiefSkillComponent::StartEnhanceScreenVFX(ATimeThiefCharacterBase& OwnerCharacter)
{
	if (!OwnerCharacter.IsLocallyControlled())
	{
		return;
	}

	TargetEnhanceScreenPostProcessStrength = 1.0f;
	if (EnhanceScreenPostProcessFadeInSeconds <= KINDA_SMALL_NUMBER)
	{
		SetEnhanceScreenPostProcessStrength(1.0f);
	}
}

void UTimeThiefSkillComponent::StopEnhanceScreenVFX(bool bImmediate)
{
	TargetEnhanceScreenPostProcessStrength = 0.0f;
	if (bImmediate || EnhanceScreenPostProcessFadeOutSeconds <= KINDA_SMALL_NUMBER)
	{
		SetEnhanceScreenPostProcessStrength(0.0f);
	}
}

void UTimeThiefSkillComponent::UpdateEnhanceScreenPostProcess(float DeltaTime)
{
	if (FMath::IsNearlyEqual(EnhanceScreenPostProcessStrength, TargetEnhanceScreenPostProcessStrength))
	{
		return;
	}

	const float FadeSeconds = TargetEnhanceScreenPostProcessStrength > EnhanceScreenPostProcessStrength
		? EnhanceScreenPostProcessFadeInSeconds
		: EnhanceScreenPostProcessFadeOutSeconds;
	const float NewStrength = FadeSeconds > KINDA_SMALL_NUMBER
		? FMath::FInterpConstantTo(
			EnhanceScreenPostProcessStrength,
			TargetEnhanceScreenPostProcessStrength,
			DeltaTime,
			1.0f / FadeSeconds)
		: TargetEnhanceScreenPostProcessStrength;

	SetEnhanceScreenPostProcessStrength(NewStrength);
}

void UTimeThiefSkillComponent::SetEnhanceScreenPostProcessStrength(float Strength)
{
	EnhanceScreenPostProcessStrength = FMath::Clamp(Strength, 0.0f, 1.0f);

	ATimeThiefCharacterBase* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled() || !EnhanceScreenPostProcessMaterial)
	{
		return;
	}
	if (!ActiveEnhanceScreenPostProcessMaterial && EnhanceScreenPostProcessStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (!ActiveEnhanceScreenPostProcessMaterial)
	{
		ActiveEnhanceScreenPostProcessMaterial = UMaterialInstanceDynamic::Create(EnhanceScreenPostProcessMaterial, this);
		if (!ActiveEnhanceScreenPostProcessMaterial)
		{
			return;
		}
	}

	ActiveEnhanceScreenPostProcessMaterial->SetScalarParameterValue(EffectStrengthParameterName, EnhanceScreenPostProcessStrength);

	TArray<UCameraComponent*> CameraComponents;
	OwnerCharacter->GetComponents<UCameraComponent>(CameraComponents);
	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		if (CameraComponent)
		{
			AddOrUpdateEnhanceScreenPostProcessBlendable(*CameraComponent);
		}
	}
}

void UTimeThiefSkillComponent::AddOrUpdateEnhanceScreenPostProcessBlendable(UCameraComponent& CameraComponent) const
{
	if (!ActiveEnhanceScreenPostProcessMaterial)
	{
		return;
	}

	TScriptInterface<IBlendableInterface> Blendable;
	Blendable.SetObject(ActiveEnhanceScreenPostProcessMaterial);
	Blendable.SetInterface(Cast<IBlendableInterface>(ActiveEnhanceScreenPostProcessMaterial));
	const float BlendableWeight = EnhanceScreenPostProcessStrength > KINDA_SMALL_NUMBER ? 1.0f : 0.0f;
	CameraComponent.AddOrUpdateBlendable(Blendable, BlendableWeight);
}

void UTimeThiefSkillComponent::StartRewindVFX(ATimeThiefCharacterBase& OwnerCharacter)
{
	StopRewindVFX();
	PlaySkillSoundAtLocation(RewindStartSound, GetMeshCenterWorldLocation(OwnerCharacter));

	if (USkeletalMeshComponent* MeshComponent = OwnerCharacter.GetMesh())
	{
		const FVector MeshCenterRelativeLocation = GetMeshCenterRelativeLocation(*MeshComponent);
		ActiveRewindTrailNiagaraComponent = PlaySkillNiagaraAttached(
			RewindTrailNiagaraEffect,
			*MeshComponent,
			MeshCenterRelativeLocation);
	}
}

void UTimeThiefSkillComponent::StopRewindVFX()
{
	StopSkillNiagaraComponent(ActiveRewindTrailNiagaraComponent);
}

void UTimeThiefSkillComponent::PlaySkillNiagaraAtLocation(UNiagaraSystem* NiagaraSystem, const FVector& Location, const FRotator& Rotation) const
{
	if (!CanSpawnSkillNiagara() || !NiagaraSystem)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, NiagaraSystem, Location, Rotation);
}

UNiagaraComponent* UTimeThiefSkillComponent::PlaySkillNiagaraAttached(UNiagaraSystem* NiagaraSystem, USceneComponent& AttachComponent, const FVector& RelativeLocation) const
{
	if (!CanSpawnSkillNiagara() || !NiagaraSystem)
	{
		return nullptr;
	}

	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		NiagaraSystem,
		&AttachComponent,
		NAME_None,
		RelativeLocation,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,
		false,
		ENCPoolMethod::AutoRelease,
		false);

	if (NiagaraComponent)
	{
		NiagaraComponent->Activate(true);
	}

	return NiagaraComponent;
}

void UTimeThiefSkillComponent::StopSkillNiagaraComponent(TObjectPtr<UNiagaraComponent>& NiagaraComponent) const
{
	if (!IsValid(NiagaraComponent))
	{
		NiagaraComponent = nullptr;
		return;
	}

	NiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	NiagaraComponent->Deactivate();
	NiagaraComponent = nullptr;
}

void UTimeThiefSkillComponent::PlaySkillSoundAtLocation(USoundBase* Sound, const FVector& Location) const
{
	if (!Sound)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
}

bool UTimeThiefSkillComponent::CanSpawnSkillNiagara() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

void UTimeThiefSkillComponent::HideRewindMeshes(ATimeThiefCharacterBase& OwnerCharacter)
{
	RestoreRewindMeshes();

	HideRewindMesh(OwnerCharacter.GetMesh());
	HideRewindMesh(OwnerCharacter.GetFirstPersonMesh());

	if (UMorphingMeshComponent* MorphingMeshComponent = OwnerCharacter.GetMorphingMeshComponent())
	{
		HideRewindMesh(MorphingMeshComponent->BaseSkeletalMeshComponent);
		HideRewindMesh(MorphingMeshComponent->BaseMeshComponent);
		HideRewindMesh(MorphingMeshComponent->LiquidMeshComponent);
	}
}

void UTimeThiefSkillComponent::HideRewindMesh(UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (RewindHiddenMeshStates.ContainsByPredicate([Component](const FTimeThiefRewindHiddenMeshState& State)
	{
		return State.Component.Get() == Component;
	}))
	{
		return;
	}

	FTimeThiefRewindHiddenMeshState State;
	State.Component = Component;
	State.bHiddenInGame = Component->bHiddenInGame;
	RewindHiddenMeshStates.Add(State);

	Component->SetHiddenInGame(true, true);
}

void UTimeThiefSkillComponent::RestoreRewindMeshes()
{
	for (const FTimeThiefRewindHiddenMeshState& State : RewindHiddenMeshStates)
	{
		if (UPrimitiveComponent* Component = State.Component.Get())
		{
			Component->SetHiddenInGame(State.bHiddenInGame, true);
		}
	}

	RewindHiddenMeshStates.Reset();
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

ATimeThiefCharacterBase* UTimeThiefSkillComponent::GetOwnerCharacter() const
{
	return Cast<ATimeThiefCharacterBase>(GetOwner());
}
