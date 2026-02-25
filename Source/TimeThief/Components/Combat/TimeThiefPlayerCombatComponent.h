#pragma once

#include "CoreMinimal.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "TimeThiefPlayerCombatComponent.generated.h"

class ATimeThiefWeaponBase;
class ATimeThiefRifle;

UCLASS()
class TIMETHIEF_API UTimeThiefPlayerCombatComponent : public UTimeThiefPawnCombatComponent {
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;


	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	ATimeThiefWeaponBase* SpawnAndRegisterWeapon(TSubclassOf<ATimeThiefWeaponBase> WeaponClass, bool bEquipImmediately = false);

	virtual void HandleInputPressed(FGameplayTag InputTag) override;
	virtual void HandleInputReleased(FGameplayTag InputTag) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TimeThief|Combat")
	TArray<TSubclassOf<ATimeThiefWeaponBase>> DefaultWeaponClasses;
};