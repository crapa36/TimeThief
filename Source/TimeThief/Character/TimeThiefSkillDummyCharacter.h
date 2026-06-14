#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "TimeThiefSkillDummyCharacter.generated.h"

class ATimeThiefCharacterBase;
class ATimeThiefMasterWeapon;
class UNiagaraSystem;
class UChildActorComponent;
class UStaticMeshComponent;
class UTimeThiefWeaponComponentBase;

UCLASS()
class TIMETHIEF_API ATimeThiefSkillDummyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATimeThiefSkillDummyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	void InitializeFromSource(ATimeThiefCharacterBase* SourceCharacter, const FVector& InMoveDirection, float InMoveSpeed, float InLifetime);
	void SetDespawnNiagaraEffect(UNiagaraSystem* InDespawnNiagaraEffect);
	const FVector& GetCopiedMeshAlpha() const { return CopiedMeshAlpha; }
	FVector GetIntendedMoveVelocity() const { return MoveDirection * MoveSpeed; }
	UStaticMeshComponent* GetCopiedWeaponMesh() const;
	UTimeThiefWeaponComponentBase* GetCopiedWeaponComponent() const { return CopiedWeaponComponent; }
	FName GetCopiedWeaponLeftHandIKSocketName() const { return CopiedWeaponLeftHandIKSocketName; }
	FGameplayTag GetCopiedWeaponTag() const { return CopiedWeaponTag; }

private:
	void ConfigureMovement();
	void ConfigureMeshFromSource(ATimeThiefCharacterBase* SourceCharacter);
	void ConfigureWeaponFromSource(ATimeThiefCharacterBase* SourceCharacter);
	void RequestForwardMove(float DeltaTime);

	UPROPERTY(VisibleAnywhere, Category = "TimeThief|Dummy")
	TObjectPtr<UChildActorComponent> CopiedWeaponActorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTimeThiefWeaponComponentBase> CopiedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> DespawnNiagaraEffect;

	FVector MoveDirection = FVector::ForwardVector;
	FVector CopiedMeshAlpha = FVector(0.0f, 1.0f, 0.0f);
	FName CopiedWeaponLeftHandIKSocketName = TEXT("LeftHandIK");
	FGameplayTag CopiedWeaponTag;
	float MoveSpeed = 0.0f;
};
