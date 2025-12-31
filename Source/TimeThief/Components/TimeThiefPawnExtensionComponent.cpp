#include "Components/TimeThiefPawnExtensionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameFramework/Pawn.h"
#include "TimeThiefGameplayTags.h"

const FName UTimeThiefPawnExtensionComponent::NAME_ActorFeatureName("PawnExtension");

UTimeThiefPawnExtensionComponent::UTimeThiefPawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
}

bool UTimeThiefPawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const {
	check(Manager);
	
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) {
		return false;
	}

	return true;
}

void UTimeThiefPawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) {
	OnPawnReadyToInitialize();
}

void UTimeThiefPawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params) {
}

void UTimeThiefPawnExtensionComponent::CheckDefaultInitialization() {
	if (UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(GetOwner())) {
		const FGameplayTag& InitTag = FTimeThiefGameplayTags::Get().InitState_Spawned;
		if (InitTag.IsValid()) {
			Manager->ChangeFeatureInitState(GetOwner(), NAME_ActorFeatureName, this, InitTag);
		}
	}
}

void UTimeThiefPawnExtensionComponent::BindOnActorInitStateChanged(FName FeatureName, FGameplayTag RequiredState, bool bCallImmediately) {
	if (UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(GetOwner())) {
		Manager->RegisterAndCallForActorInitState(
			GetOwner(),
			FeatureName,
			RequiredState,
			FActorInitStateChangedDelegate::CreateUObject(this, &ThisClass::OnActorInitStateChanged),
			bCallImmediately
		);
	}
}

