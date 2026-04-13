#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"

#include "SendBuffer.h"

class FSocket;
class SendBuffer;

/*-----------------
   PacketSession
-----------------*/
//
// PacketSession는 클라이언트 측의 세션을 담당하는 클래스입니다.
//

class TIMETHIEF_API PacketSession : public TSharedFromThis<PacketSession>
{
public:
	PacketSession(FSocket* Socket);
	~PacketSession();
	
public:
	void Run();
	
	UFUNCTION(BlueprintCallable)
	void HandleRecvPackets();
	
	void SendPacket(TSharedPtr<SendBuffer> SendBuffer);
	
	void Disconnect();
	
private:
	void TryAssemblePackets();
	
public:
	FSocket*						Socket;
	TAtomic<bool>					bDisconnected{false};
	
	// TODO: Network Worker 먼저 만들기
	TSharedPtr<class RecvWorker>	RecvWorkerThread;
	TSharedPtr<class SendWorker>	SendWorkerThread;
	
	// SPSC Queue (Single Producer Single Consumer)
	// 아래 위험..? (EQueueMode 설정해야 할 수도)
	TQueue<TArray<uint8>>			RecvPacketQueue;
	TQueue<TSharedPtr<SendBuffer>>	SendPacketQueue;
	
	TArray<uint8> RecvStreamBuffer;
	
};
