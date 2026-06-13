#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "TimeThiefSkillComponent.generated.h"

class ATimeThiefCharacterBase;
class ATimeThiefSkillDummyCharacter;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ETimeThiefSkillType : uint8
{
	None,
	Enhance,
	Rewind,
	Dummy,
};

USTRUCT(BlueprintType)
struct FTimeThiefSkillTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float EnhanceDurationSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float EnhanceMoveSpeedMultiplier = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float EnhanceFireRateMultiplier = 1.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float EnhanceReloadSpeedMultiplier = 1.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float EnhanceDamageMultiplier = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float EnhanceEquipSpeedMultiplier = 1.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float RewindPlaybackSeconds = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float RewindHistorySeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float RewindSnapshotIntervalSeconds = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill")
	float DummyLifetimeSeconds = 3.0f;
};

USTRUCT(BlueprintType)
struct FTimeThiefSkillSlotState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Skill")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Skill")
	int32 SkillId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TimeThief|Skill")
	ETimeThiefSkillType SkillType = ETimeThiefSkillType::None;
};

USTRUCT()
struct FTimeThiefRewindSnapshot
{
	GENERATED_BODY()

	float TimeSeconds = 0.0f;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Health = 0.0f;
	FGameplayTag ActiveWeaponTag;
	int32 CurrentAmmo = 0;
};

struct FTimeThiefRewindHiddenMeshState
{
	TWeakObjectPtr<UPrimitiveComponent> Component;
	bool bHiddenInGame = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeThiefSkillComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool HandleSkillInput(FGameplayTag InputTag);
	void ApplySkillSnapshot(const TArray<FTimeThiefSkillSlotState>& InEquippedSlots);
	void SetEquippedSkillSlot(uint32 SlotIndex, uint32 SkillId);
	bool FindEquippedSkillSlot(uint32 SkillId, uint32& OutSlotIndex) const;
	bool FindFirstAvailableSkillSlot(uint32& OutSlotIndex) const;

	void ApplyTimeAccelEffect(uint32 DurationMs, uint32 FireRateBonusPercent, uint32 MoveSpeedBonusPercent);
	void SpawnDummyEffect(const FVector& StartPosition, const FVector& Direction, float MoveSpeed, uint32 DurationMs);
	void ApplyRewindEffect(uint32 DurationMs, uint32 RewindDurationMs, int32 TargetHealth, const FVector& TargetPosition, bool bHasTargetPosition);

	bool IsRewinding() const { return bRewinding; }
	bool IsEnhanceActive() const { return bEnhanceActive; }

	float GetMoveSpeedMultiplier() const;
	float GetFireRateMultiplier() const;
	float GetReloadSpeedMultiplier() const;
	float GetDamageMultiplier() const;
	float GetEquipSpeedMultiplier() const;

	static ETimeThiefSkillType ResolveSkillTypeFromId(uint32 SkillId);

private:
	bool TryUseSkillSlot(uint32 SlotIndex);
	bool TryUseSkill(ETimeThiefSkillType SkillType, uint32 SlotIndex, uint32 SkillId);
	const FTimeThiefSkillSlotState* FindSlot(uint32 SlotIndex) const;

	FTimeThiefRewindSnapshot CaptureRewindSnapshot() const;
	void RecordRewindSnapshot();
	void TickRewind(float DeltaTime);
	bool BuildRewindPathFromHistory(float TargetSecondsAgo);
	void FinishRewind();
	void ApplyFinalRewindSnapshot(const FTimeThiefRewindSnapshot& Snapshot);

	void StartEnhance(float DurationSeconds, float MoveSpeedMultiplier, float FireRateMultiplier, float ReloadSpeedMultiplier, float DamageMultiplier, float EquipSpeedMultiplier);
	void StopEnhance();
	void StartEnhanceVFX(ATimeThiefCharacterBase& OwnerCharacter);
	void StopEnhanceVFX();
	void StartRewindVFX(ATimeThiefCharacterBase& OwnerCharacter);
	void StopRewindVFX();
	void PlaySkillNiagaraAtLocation(UNiagaraSystem* NiagaraSystem, const FVector& Location, const FRotator& Rotation) const;
	UNiagaraComponent* PlaySkillNiagaraAttached(UNiagaraSystem* NiagaraSystem, USceneComponent& AttachComponent, const FVector& RelativeLocation) const;
	void StopSkillNiagaraComponent(TObjectPtr<UNiagaraComponent>& NiagaraComponent) const;
	bool CanSpawnSkillNiagara() const;
	void HideRewindMeshes(ATimeThiefCharacterBase& OwnerCharacter);
	void HideRewindMesh(UPrimitiveComponent* Component);
	void RestoreRewindMeshes();
	void RefreshSkillModifiedStats() const;

	ATimeThiefCharacterBase* GetOwnerCharacter() const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Skill")
	FTimeThiefSkillTuning Tuning;

	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Skill|Dummy")
	TSubclassOf<ATimeThiefSkillDummyCharacter> DummyClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill|Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> EnhanceAuraNiagaraEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill|Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> EnhanceStartNiagaraEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill|Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> EnhanceScreenNiagaraEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill|Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> DummySpawnNiagaraEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill|Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> DummyDespawnNiagaraEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill|Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> RewindTrailNiagaraEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TimeThief|Skill|Effects", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> RewindArrivalNiagaraEffect;

	UPROPERTY(Transient)
	TArray<FTimeThiefSkillSlotState> SkillSlots;

	TArray<FTimeThiefRewindSnapshot> RewindSnapshots;
	TArray<FTimeThiefRewindSnapshot> ActiveRewindPath;
	TArray<FTimeThiefRewindHiddenMeshState> RewindHiddenMeshStates;

	FTimerHandle EnhanceTimerHandle;

	float SnapshotAccumulatorSeconds = 0.0f;
	float RewindElapsedSeconds = 0.0f;
	float RewindPlaybackSeconds = 0.0f;
	EMovementMode SavedMovementMode = MOVE_Walking;
	float ActiveMoveSpeedMultiplier = 1.0f;
	float ActiveFireRateMultiplier = 1.0f;
	float ActiveReloadSpeedMultiplier = 1.0f;
	float ActiveDamageMultiplier = 1.0f;
	float ActiveEquipSpeedMultiplier = 1.0f;
	bool bEnhanceActive = false;
	bool bRewinding = false;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveEnhanceAuraNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveEnhanceScreenNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveRewindTrailNiagaraComponent;
};
