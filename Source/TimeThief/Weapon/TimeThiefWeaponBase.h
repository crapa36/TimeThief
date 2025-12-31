#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TimeThiefWeaponBase.generated.h"

class UAnimInstance;
class UAnimMontage;

UCLASS()
class TIMETHIEF_API ATimeThiefWeaponBase : public AActor {
	GENERATED_BODY()

public:
	ATimeThiefWeaponBase();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	FGameplayTag GetWeaponTag() const { return WeaponTag; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	FName GetSocketName() const { return SocketName; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	TSubclassOf<UAnimInstance> GetEquipAnimLayer() const { return EquipAnimLayer; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	UAnimMontage* GetEquipMontage() const { return EquipMontage; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	UAnimMontage* GetUnequipMontage() const { return UnequipMontage; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName SocketName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> EquipAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> UnequipMontage;
};
