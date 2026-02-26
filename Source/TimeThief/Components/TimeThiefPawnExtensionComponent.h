#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "TimeThiefPawnExtensionComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TIMETHIEF_API UTimeThiefPawnExtensionComponent : public UActorComponent, public IGameFrameworkInitStateInterface {
	GENERATED_BODY()

public:
	static const FName NAME_ActorFeatureName;

	UTimeThiefPawnExtensionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;

protected:
	template <class T>
	T* GetPawn() const {
		static_assert(TPointerIsConvertibleFromTo<T, APawn>::Value, "T must be derived from APawn");
		return Cast<T>(GetOwner());
	}

	APawn* GetPawn() const {
		return GetPawn<APawn>();
	}

	template <class T>
	T* GetController() const {
		static_assert(TPointerIsConvertibleFromTo<T, AController>::Value, "T must be derived from AController");
		APawn* Pawn = GetPawn<APawn>();
		return Pawn ? Pawn->GetController<T>() : nullptr;
	}

	void BindOnActorInitStateChanged(FName FeatureName, FGameplayTag RequiredState, bool bCallImmediately);

	virtual void OnPawnReadyToInitialize() {}
};