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

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FGameplayTag GetWeaponTag() const { return WeaponTag; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FName GetSocketName() const { return SocketName; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	TSubclassOf<UAnimInstance> GetEquipAnimLayer() const { return EquipAnimLayer; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Animation")
	UAnimMontage* GetEquipMontage() const { return EquipMontage; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Animation")
	UAnimMontage* GetUnequipMontage() const { return UnequipMontage; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Socket")
	FName GetMuzzleSocketName() const { return MuzzleSocketName; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Socket")
	FName GetGripSocketName() const { return GripSocketName; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Socket")
	FName GetLeftHandSocketName() const { return LeftHandSocketName; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Socket")
	FTransform GetSocketTransformByName(FName InSocketName) const;

	UFUNCTION(BlueprintPure, Category = "Weapon|Socket")
	FTransform GetGripOffset() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName SocketName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Socket")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Socket")
	FName GripSocketName = TEXT("Grip");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Socket")
	FName LeftHandSocketName = TEXT("LeftHand");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> EquipAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> UnequipMontage;
};
