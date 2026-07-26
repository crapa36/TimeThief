


#include "NetworkEntityComponent.h"

UNetworkEntityComponent::UNetworkEntityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNetworkEntityComponent::SetControlType(ENetworkControlType InControlType)
{
	if (ControlType == InControlType)
	{
		return;
	}

	ControlType = InControlType;
	OnControlTypeChanged.Broadcast(ControlType);
}

bool UNetworkEntityComponent::CanSendInput() const
{
	return ControlType == ENetworkControlType::Local;
}

bool UNetworkEntityComponent::ShouldApplyNetworkState() const
{
	return ControlType != ENetworkControlType::Local;
}

void UNetworkEntityComponent::ResetNetworkEntity()
{
	EntityId = 0;
	SetControlType(ENetworkControlType::None);
}
