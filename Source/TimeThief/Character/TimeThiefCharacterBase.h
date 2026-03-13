#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimeThiefCharacterBase.generated.h"

class UTimePointSystemComponent;
class UTimeThiefPawnCombatComponent;
class UTimeThiefHealthComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
struct FStoreOrder;

UCLASS()
class TIMETHIEF_API ATimeThiefCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ATimeThiefCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	bool PurchaseItem(const FStoreOrder& Order);
	
	UFUNCTION(BlueprintCallable, Category = "TimeThief|Combat")
	virtual UTimeThiefPawnCombatComponent* GetPawnCombatComponent() const { return nullptr; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Health")
	UTimeThiefHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Camera")
	void TogglePerspective();

	UFUNCTION(BlueprintCallable, Category = "TimeThief|Mesh")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	
protected:
	virtual void BeginPlay() override;

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
	
	bool bIsFirstPerson = false;
};
