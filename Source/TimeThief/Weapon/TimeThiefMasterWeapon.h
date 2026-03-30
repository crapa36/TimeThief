#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "MorphingMesh/MorphingMeshComponent.h"
#include "TimeThiefMasterWeapon.generated.h"

class UStaticMeshComponent;
class UTimeThiefWeaponComponentBase;
class UTimeThiefRifleComponent;
class UTimeThiefShotgunComponent;
class UTimeThiefRocketLauncherComponent;
class UStaticMesh;

UCLASS()
class TIMETHIEF_API ATimeThiefMasterWeapon : public AActor {
	GENERATED_BODY()

public:
	ATimeThiefMasterWeapon();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	void SwitchWeapon(FGameplayTag WeaponTag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Weapon")
	void Reload();

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	UStaticMeshComponent* GetWeaponMesh() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	UStaticMesh* GetActiveStaticMesh() const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	UTimeThiefWeaponComponentBase* GetActiveWeaponComponent() const { return ActiveWeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon")
	UTimeThiefWeaponComponentBase* GetWeaponComponentByTag(FGameplayTag WeaponTag) const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon|Socket")
	FTransform GetSocketTransform(FName SocketName, ERelativeTransformSpace TransformSpace = RTS_World) const;

	UFUNCTION(BlueprintPure, Category = "TimeThief|Weapon|Socket")
	FVector GetSocketLocation(FName SocketName) const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UMorphingMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<UTimeThiefRifleComponent> RifleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<UTimeThiefShotgunComponent> ShotgunComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<UTimeThiefRocketLauncherComponent> RocketLauncherComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|Runtime")
	TObjectPtr<UTimeThiefWeaponComponentBase> ActiveWeaponComponent;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UTimeThiefWeaponComponentBase>> WeaponComponents;
	
	private:
	EMorphTargetType GetMorphTargetTypeByTag(FGameplayTag WeaponTag) const;
};