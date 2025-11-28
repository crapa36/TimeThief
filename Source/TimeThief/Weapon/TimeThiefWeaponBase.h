#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TimeThiefWeaponBase.generated.h"

class USkeletalMeshComponent;
class UGameplayAbility;
class UAnimInstance;

UCLASS()
class TIMETHIEF_API ATimeThiefWeaponBase : public AActor {
	GENERATED_BODY()

public:
	ATimeThiefWeaponBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	FGameplayTag WeaponTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TSubclassOf<UAnimInstance> EquipAnimLayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	FName SocketName;

public:
	FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	FORCEINLINE FGameplayTag GetWeaponTag() const { return WeaponTag; }
	FORCEINLINE TSubclassOf<UAnimInstance> GetEquipAnimLayer() const { return EquipAnimLayer; }
	FORCEINLINE TArray<TSubclassOf<UGameplayAbility>> GetDefaultAbilities() const { return DefaultAbilities; }
	FORCEINLINE FName GetSocketName() const { return SocketName; }
};