#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"

#include "SendBuffer.h"

class FSocket;
class SendBuffer;

/*-----------------
   ClientSession
-----------------*/
//
// ClientSession는 클라이언트 측의 세션을 담당하는 클래스입니다.
//

class TIMETHIEF_API ClientSession : public TSharedFromThis<ClientSession>
{
public:
	ClientSession(class FSocket* Socket);
	~ClientSession();
	
public:
	void Run();
	
	UFUNCTION(BlueprintCallable)
	void HandleRecvPackets();
	
	void SendPacket(TSharedPtr<SendBuffer> SendBuffer);
	
	void Disconnect();
	
public:
	FSocket*						Socket;
	
	// TODO: Network Worker 먼저 만들기
	TSharedPtr<class RecvWorker>	RecvWorkerThread;
	TSharedPtr<class SendWorker>	SendWorkerThread;
	
	// SPSC Queue (Single Producer Single Consumer)
	TQueue<TArray<uint8>>			RecvPacketQueue;
	TQueue<TSharedPtr<SendBuffer>>	SendPacketQueue;
	
};
