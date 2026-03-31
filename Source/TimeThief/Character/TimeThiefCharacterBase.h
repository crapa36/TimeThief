#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "TimeThiefCharacterBase.generated.h"

class UTimePointSystemComponent;
class UTimeThiefPawnCombatComponent;
class UTimeThiefHealthComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
struct FStoreOrder;

class UAnimMontage;
class UAnimSequenceBase;
class UNiagaraComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	UFUNCTION(BlueprintCallable)
	void SetMask(float NewMask);
	
	void AddMask(float Amount);
	
	virtual UTimeThiefPawnCombatComponent* GetCombatComponent() const { return nullptr; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual USkeletalMeshComponent* GetWeaponAttachMesh() const { return GetMesh(); }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual USkeletalMeshComponent* GetMontagePlaybackMesh() const { return GetMesh(); }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	UTimeThiefHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Camera")
	void TogglePerspective();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Mesh")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Animation")
	void PlayMontageOnAllMeshes(UAnimMontage* Montage, float PlayRate = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Animation")
	void PlayAnimationOnAllMeshes(UAnimSequenceBase* Animation, FName SlotName = FName("DefaultSlot"), float BlendInTime = 0.15f, float BlendOutTime = 0.15f, float PlayRate = 1.0f);

	UFUNCTION(BlueprintPure, Category = "TimeThief|Camera")
	bool IsFirstPerson() const { return bIsFirstPerson; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Tags")
	void AddOwnedGameplayTag(const FGameplayTag& Tag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Tags")
	void RemoveOwnedGameplayTag(const FGameplayTag& Tag);

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Tags")
	bool HasOwnedGameplayTag(const FGameplayTag& Tag) const;

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Tags")
	const FGameplayTagContainer& GetOwnedGameplayTags() const { return OwnedGameplayTags; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Tags")
	void AppendOwnedGameplayTags(const FGameplayTagContainer& InTags);

	virtual void OnPlayerInitialized();

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	
	virtual void ApplyPerspective();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|TimePoint")
	TObjectPtr<UTimePointSystemComponent> TimePointSystemComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UTimeThiefHealthComponent> HealthComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = "Mesh")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> FirstPersonSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UNiagaraComponent> DisappearFX;
	
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	bool bIsFirstPerson = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer OwnedGameplayTags;
	
	float Mask = 1;
	
private:
	void UpdateMask();
};