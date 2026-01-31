#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimeThiefCharacterBase.generated.h"

class UTimeThiefPawnCombatComponent;
class UCameraComponent;
class USkeletalMeshComponent;

UCLASS()
class TIMETHIEF_API ATimeThiefCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual UTimeThiefPawnCombatComponent* GetPawnCombatComponent() const { return nullptr; }

	UFUNCTION(BlueprintPure, Category = "TimeThief|Character")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Camera")
	void TogglePerspective();

protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	bool bIsFirstPerson = false;
};
