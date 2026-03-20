


#include "NetworkCombatSyncComponent.h"

#include <Generated/ClientPacketHandler.h>

#include "NetworkGameInstanceSubsystem.h"
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
	
	TTCombatComponent = GetOwner()->FindComponentByClass<UTimeThiefPawnCombatComponent>();
	if (TTCombatComponent)
	{
		// TODO: TTPCC에 공격에 관한 델리게이트가 생기면 그쪽에 바인딩
		//		 Fire, Reload, Throw ... 등의 공격이 발생할 때 해당 델리게이트가 발동 되어야 함
		// TTCombatComponent->OnLocalAttackRequest.AddUObject ...
	}
	
}

void UNetworkCombatSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TTCombatComponent)
	{
		// TODO: TTPCC에 바인딩 되어 있던 델리게이트 언바인딩
		 // TTCombatComponent->OnLocalAttackRequest.RemoveAll(this);
	}
	
	Super::EndPlay(EndPlayReason);
}

void UNetworkCombatSyncComponent::HandleLocalAttackRequest(const FCombatAttackRequest& AttackRequest)
{
	SendBufferRef Buffer;
	switch (AttackRequest.NotifyType)
	{
	case Fire:
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
			
			Buffer = ClientPacketHandler::MakeSendBuffer(Request);
		}
		break;
	case Throw:
		{
			se::game::C_ThrowGrenadeReq Request;
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
	case Reload:
		{
			se::game::C_ReloadReq Request;
			Request.set_weapon_id(AttackRequest.WeaponId);
			
			Buffer = ClientPacketHandler::MakeSendBuffer(Request);
		}
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("HandleLocalAttackRequest: Unknown AttackId %u"), AttackRequest.AttackId);
		break;
	}
	
	if (Buffer)
	{
		if (UNetworkGameInstanceSubsystem* NGIS = UNetworkGameInstanceSubsystem::Get(this))
		{
			NGIS->SendPacket(Buffer);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("HandleLocalAttackRequest: Failed to get NetworkGameInstanceSubsystem"));
		}
	}
}

void UNetworkCombatSyncComponent::BroadcastRemoteAttackNotify(const FRemoteAttackNotify& AttackNotify) const
{
	OnRemoteAttackNotify.Broadcast(AttackNotify);
}
