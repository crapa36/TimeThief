#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimeThiefSkillDummyCharacter.generated.h"

class ATimeThiefCharacterBase;
class UStaticMeshComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefSkillDummyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ATimeThiefSkillDummyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaTime) override;

	void InitializeFromSource(ATimeThiefCharacterBase* SourceCharacter, const FVector& InMoveDirection, float InMoveSpeed, float InLifetime);
	const FVector& GetCopiedMeshAlpha() const { return CopiedMeshAlpha; }
	FVector GetIntendedMoveVelocity() const { return MoveDirection * MoveSpeed; }

private:
	void ConfigureMovement();
	void ConfigureMeshFromSource(ATimeThiefCharacterBase* SourceCharacter);
	void ConfigureWeaponFromSource(ATimeThiefCharacterBase* SourceCharacter);
	void RequestForwardMove(float DeltaTime);

	UPROPERTY(VisibleAnywhere, Category = "TimeThief|Dummy")
	TObjectPtr<UStaticMeshComponent> CopiedWeaponMesh;

	FVector MoveDirection = FVector::ForwardVector;
	FVector CopiedMeshAlpha = FVector(0.0f, 1.0f, 0.0f);
	float MoveSpeed = 0.0f;
};
