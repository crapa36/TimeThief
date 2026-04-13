#include "Network/NetworkWireComponent.h"

#include "Components/Wire/TimeThiefWireComponent.h"
#include "Network/NetworkGameInstanceSubsystem.h"
#include "GameFramework/Pawn.h"

UNetworkWireComponent::UNetworkWireComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNetworkWireComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		WireComponent = Owner->FindComponentByClass<UTimeThiefWireComponent>();
	}

	if (WireComponent)
	{
		WireComponent->OnWireAttached.AddDynamic(this, &UNetworkWireComponent::HandleLocalWireAttached);
		WireComponent->OnWireStateChanged.AddDynamic(this, &UNetworkWireComponent::HandleLocalWireStateChanged);
	}

	if (GetNetworkGameInstanceSubsystem() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NetworkWire] NGIS cache failed in BeginPlay. Owner=%s"), *GetNameSafe(GetOwner()));
	}
}

void UNetworkWireComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WireComponent)
	{
		WireComponent->OnWireAttached.RemoveDynamic(this, &UNetworkWireComponent::HandleLocalWireAttached);
		WireComponent->OnWireStateChanged.RemoveDynamic(this, &UNetworkWireComponent::HandleLocalWireStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UNetworkWireComponent::HandleLocalWireAttached(const FVector& AnchorPoint)
{
	if (!IsLocalControlledOwner())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=SendLocal][OnWireAttached] Owner=%s Anchor=(%.1f, %.1f, %.1f)"), *GetNameSafe(GetOwner()), AnchorPoint.X, AnchorPoint.Y, AnchorPoint.Z);

	if (UNetworkGameInstanceSubsystem* NetworkSubsystem = GetNetworkGameInstanceSubsystem())
	{
		NetworkSubsystem->SendWireAction(AnchorPoint);
	}
}

void UNetworkWireComponent::HandleLocalWireStateChanged(EWireState OldState, EWireState NewState)
{
	if (!IsLocalControlledOwner())
	{
		return;
	}

	if (OldState == EWireState::Attached && NewState != EWireState::Attached)
	{
		UE_LOG(LogTemp, Log, TEXT("[WirePkt][Stage=SendLocal][OnWireStateChanged] Owner=%s Old=%d New=%d"), *GetNameSafe(GetOwner()), static_cast<int32>(OldState), static_cast<int32>(NewState));

		if (UNetworkGameInstanceSubsystem* NetworkSubsystem = GetNetworkGameInstanceSubsystem())
		{
			NetworkSubsystem->SendWireActionEnd();
		}
	}
}

bool UNetworkWireComponent::IsLocalControlledOwner() const
{
	if (const APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		return PawnOwner->IsLocallyControlled();
	}

	return false;
}

UNetworkGameInstanceSubsystem* UNetworkWireComponent::GetNetworkGameInstanceSubsystem()
{
	if (NGIS)
	{
		return NGIS;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			NGIS = GameInstance->GetSubsystem<UNetworkGameInstanceSubsystem>();
		}
	}

	return NGIS;
}



