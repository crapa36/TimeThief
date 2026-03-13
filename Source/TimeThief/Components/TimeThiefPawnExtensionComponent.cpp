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

	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	if (DesiredState == Tags.InitState_Spawned)
	{
		return true;
	}

	if (DesiredState == Tags.InitState_DataAvailable)
	{
		return CurrentState == Tags.InitState_Spawned && bPawnDataSet;
	}

	if (DesiredState == Tags.InitState_DataInitialized)
	{
		return CurrentState == Tags.InitState_DataAvailable && bInputsReady;
	}

	if (DesiredState == Tags.InitState_GameplayReady)
	{
		return CurrentState == Tags.InitState_DataInitialized;
	}

	return false;
}

void UTimeThiefPawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) {
	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	CurrentInitState = DesiredState;

	if (DesiredState == Tags.InitState_DataAvailable)
	{
		OnPawnReadyToInitialize();
	}
	else if (DesiredState == Tags.InitState_GameplayReady)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] PawnExtension reached GameplayReady state."), *GetNameSafe(GetOwner()));
	}
}

void UTimeThiefPawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params) {
}

void UTimeThiefPawnExtensionComponent::CheckDefaultInitialization() {
	UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(GetOwner());
	if (!Manager) {
		return;
	}

	const FTimeThiefGameplayTags& Tags = FTimeThiefGameplayTags::Get();

	static const TArray<FGameplayTag> StateChain = {
		Tags.InitState_Spawned,
		Tags.InitState_DataAvailable,
		Tags.InitState_DataInitialized,
		Tags.InitState_GameplayReady
	};

	for (const FGameplayTag& State : StateChain)
	{
		if (!TryTransitionToState(Manager, State))
		{
			break;
		}
	}
}

bool UTimeThiefPawnExtensionComponent::TryTransitionToState(UGameFrameworkComponentManager* Manager, const FGameplayTag& DesiredState)
{
	if (CurrentInitState == DesiredState)
	{
		return true;
	}

	if (CanChangeInitState(Manager, CurrentInitState, DesiredState))
	{
		Manager->ChangeFeatureInitState(GetOwner(), NAME_ActorFeatureName, this, DesiredState);
		return true;
	}

	return false;
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

