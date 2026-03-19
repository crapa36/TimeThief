#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Sockets.h"

#include "PacketSession.h"
#include "SendBuffer.h"

class FSocket;
class PacketSession;
class SendBuffer;

/*--------------
   RecvWorker
--------------*/
//
// RecvWorker는 수신 작업을 처리하는 네트워크 워커 클래스입니다.
//

class TIMETHIEF_API RecvWorker : public FRunnable
{
public:
	RecvWorker(FSocket* Socket, TSharedPtr<PacketSession> Session);
	virtual ~RecvWorker();
	
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	
	void Destroy();
	
private:
	bool ReceivePacket(TArray<uint8>& OutPacket);
	bool ReceiveDesiredBytes(uint8* Result, int32 Size);
	
protected:
	FRunnableThread* Thread = nullptr;
	bool Running = true;
	FSocket* Socket;
	TWeakPtr<PacketSession> SessionRef;

};

/*--------------
   SendWorker
--------------*/
//
// SendWorker는 송신 작업을 처리하는 네트워크 워커 클래스입니다.
//

class TIMETHIEF_API SendWorker : public FRunnable
{
public:
	SendWorker(FSocket* Socket, TSharedPtr<PacketSession> Session);
	virtual ~SendWorker();
	
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	
	void Destroy();
	
public:
	bool SendPacket(TSharedPtr<SendBuffer> SendBuffer);
	
private:
	bool SendDesiredBytes(const uint8* Buffer, int32 Size);
	
protected:
	FRunnableThread* Thread = nullptr;
	bool Running = true;
	FSocket* Socket;
	TWeakPtr<PacketSession> SessionRef;
	
};