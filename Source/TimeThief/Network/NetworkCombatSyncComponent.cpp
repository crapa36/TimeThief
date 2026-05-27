


#include "NetworkCombatSyncComponent.h"

#include <Generated/ClientPacketHandler.h>

#include "NetworkGameInstanceSubsystem.h"
#include "Network/NetworkEntityComponent.h"
#include "Components/Combat/TimeThiefPawnCombatComponent.h"
#include "State/CombatAttackRequest.h"
#include "Protocol.pb.h"


// Sets default values for this component's properties
UNetworkCombatSyncComponent::UNetworkCombatSyncComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UNetworkCombatSyncComponent::BeginPlay()
{
	Super::BeginPlay();
	
	AActor* Owner = GetOwner();
	if (Owner)
	{
		TTCombatComponent = Owner->FindComponentByClass<UTimeThiefPawnCombatComponent>();
		NetworkEntityComponent = Owner->FindComponentByClass<UNetworkEntityComponent>();
	}

	NGIS = UNetworkGameInstanceSubsystem::Get(this);
	
	if (TTCombatComponent)
	{
		TTCombatComponent->OnCombatAttackRequest_Delegate.AddUObject(this, &UNetworkCombatSyncComponent::HandleLocalAttackRequest);
	}
}

void UNetworkCombatSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TTCombatComponent)
	{
		// TODO: TTPCC에 바인딩 되어 있던 델리게이트 언바인딩
		 // TTCombatComponent->OnLocalAttackRequest.RemoveAll(this);
	}
	
	TTCombatComponent = nullptr;
	NetworkEntityComponent = nullptr;
	NGIS = nullptr;
	
	Super::EndPlay(EndPlayReason);
}

void UNetworkCombatSyncComponent::HandleLocalAttackRequest(const FCombatAttackRequest& AttackRequest)
{
	if (!CanSendCombatPacket())
	{
		return;
	}
	
	SendBufferRef Buffer;
	switch (AttackRequest.NotifyType)
	{
	case ECombatNotifyType::Fire:
		{
			se::game::C_FireReq Request;
			Request.set_weapon_id(AttackRequest.WeaponId);
			auto* StartPos = Request.mutable_start_position();
			StartPos->set_x(AttackRequest.Origin.X);
			StartPos->set_y(AttackRequest.Origin.Y);
			StartPos->set_z(AttackRequest.Origin.Z);
			auto* Dir = Request.mutable_direction();
			Dir->set_x(AttackRequest.Direction.X);
			Dir->set_y(AttackRequest.Direction.Y);
			Dir->set_z(AttackRequest.Direction.Z);
			Request.set_shot_seed(AttackRequest.ShotSeed);
			
			Buffer = ClientPacketHandler::MakeSendBuffer(Request);
		}
		break;
	case ECombatNotifyType::Throw:
		{
			se::game::C_ThrowGrenadeReq Request;
			Request.set_grenade_type(AttackRequest.WeaponId);
			auto* StartPos = Request.mutable_start_position();
			StartPos->set_x(AttackRequest.Origin.X);
			StartPos->set_y(AttackRequest.Origin.Y);
			StartPos->set_z(AttackRequest.Origin.Z);
			auto* Dir = Request.mutable_direction();
			Dir->set_x(AttackRequest.Direction.X);
			Dir->set_y(AttackRequest.Direction.Y);
			Dir->set_z(AttackRequest.Direction.Z);
			
			Buffer = ClientPacketHandler::MakeSendBuffer(Request);
		}
		break;
	case ECombatNotifyType::WeaponChange:
		{
			se::game::C_WeaponChangeReq Request;
			Request.set_weapon_id(AttackRequest.WeaponId);
			
			Buffer = ClientPacketHandler::MakeSendBuffer(Request);
		}
		break;
	case ECombatNotifyType::Aiming:
		{
			se::game::C_AimReq Request;
			Request.set_is_aiming(true);
			
			Buffer = ClientPacketHandler::MakeSendBuffer(Request);
		}
		break;
	case ECombatNotifyType::Readying:
		{
			se::game::C_AimReq Request;
			Request.set_is_aiming(false);
			
			Buffer = ClientPacketHandler::MakeSendBuffer(Request);
		}
		break;
	case ECombatNotifyType::Reload:
		{
			se::game::C_ReloadReq Request;
			Request.set_weapon_id(AttackRequest.WeaponId);
			
			Buffer = ClientPacketHandler::MakeSendBuffer(Request);
		}
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("HandleLocalAttackRequest: Unknown AttackId %u"), AttackRequest.AttackId);
		return;;
	}
	
	if (!Buffer)
	{
		return;
	}
	
	if (UNetworkGameInstanceSubsystem* NetworkGIS = GetNetworkGameInstanceSubsystem())
	{
		NetworkGIS->SendPacket(Buffer);
	}
}

void UNetworkCombatSyncComponent::BroadcastRemoteAttackNotify(const FRemoteAttackNotify& AttackNotify) const
{
	OnRemoteAttackNotify.Broadcast(AttackNotify);
}

bool UNetworkCombatSyncComponent::CanSendCombatPacket() const
{
	const UNetworkEntityComponent* EntityComp = GetNetworkEntityComponent();
	if (EntityComp == nullptr)
	{
		return false;
	}
	
	if (!EntityComp->IsLocalControlled())
	{
		return false;
	}
	
	if (!EntityComp->IsValidEntity())
	{
		return false;
	}
	
	const UNetworkGameInstanceSubsystem* NetworkGIS = GetNetworkGameInstanceSubsystem();
	if (NetworkGIS == nullptr)
	{
		return false;
	}
	
	return NetworkGIS->CanSendGameplayPacket();
}

class UNetworkEntityComponent* UNetworkCombatSyncComponent::GetNetworkEntityComponent() const
{
	if (NetworkEntityComponent)
	{
		return NetworkEntityComponent;
	}
	
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return nullptr;
	}
	
	return Owner->FindComponentByClass<UNetworkEntityComponent>();
}

class UNetworkGameInstanceSubsystem* UNetworkCombatSyncComponent::GetNetworkGameInstanceSubsystem() const
{
	if (NGIS)
	{
		return NGIS;
	}
	
	return UNetworkGameInstanceSubsystem::Get(const_cast<UNetworkCombatSyncComponent*>(this));
}
