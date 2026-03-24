


#include "NetworkEntityComponent.h"

UNetworkEntityComponent::UNetworkEntityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	ControlType = ENetworkControlType::None;
}
