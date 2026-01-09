#include "NetworkWorker.h"

/*--------------
   RecvWorker
--------------*/

RecvWorker::RecvWorker(FSocket* Socket, TSharedPtr<ClientSession> Session)
{
	Thread = FRunnableThread::Create(this, TEXT("RecvWorkerThread"));
}

RecvWorker::~RecvWorker()
{
}

bool RecvWorker::Init()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("RecvWorker::Init()"));
	
	return FRunnable::Init();
}

uint32 RecvWorker::Run()
{
	while (Running)
	{
		TArray<uint8> Packet;
		
		if (ReceivePacket(OUT Packet))
		{
			if (TSharedPtr<ClientSession> Session = SessionRef.Pin())
			{
				Session->RecvWorkerQueue.Enqueue(Packet);
			}
		}
	}
	
	return 0;
}

void RecvWorker::Exit()
{
	FRunnable::Exit();
}

void RecvWorker::Destroy()
{
	Running = false;
}

bool RecvWorker::ReceivePacket(TArray<uint8>& OutPacket)
{
	// TEMP: PacketHeader Size는 4라고 정의
	const int32 HeaderSize = 4;
	TArray<uint8> HeaderBuffer;
	HeaderBuffer.AddZeroed(HeaderSize);
	
	if (not ReceiveDesiredBytes(HeaderBuffer.GetData(), HeaderSize))
		return false;
	
	// TODO: PacketHeader를 먼저 작성하고 진행
	// 1. PacketHeader를 읽어서 로그를 찍기
	// 2. PayloadSize를 계산해서 Payload를 읽기
	// 3. 성공했다면 OutPacket에 Header + Payload를 합쳐서 반환하기
	
	return false;
}

bool RecvWorker::ReceiveDesiredBytes(uint8* Result, int32 Size)
{
	uint32 PendingDataSize = 0;
	if (Socket->HasPendingData(PendingDataSize) == false or PendingDataSize <= 0)
		return false;
	
	int32 Offset = 0;

	while (Size > 0)
	{
		int32 NumRead = 0;
		Socket->Recv(Result + Offset, Size, OUT NumRead);
		check(NumRead <= Size);
		
		if (NumRead <= 0)
			return false;
		
		Offset += NumRead;
		Size -= NumRead;
	}
	
	return true;
}

/*--------------
   SendWorker
--------------*/

SendWorker::SendWorker(FSocket* Socket, TSharedPtr<ClientSession> Session)
{
	Thread = FRunnableThread::Create(this, TEXT("SendWorkerThread"));
}

SendWorker::~SendWorker()
{
}

bool SendWorker::Init()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("SendWorker::Init()"));
	
	return FRunnable::Init();
}

uint32 SendWorker::Run()
{
	while (Running)
	{
		TSharedPtr<SendBuffer> SendBuffer;;
		
		if (TSharedPtr<ClientSession> Session = SessionRef.Pin())
		{
			if (Session->SendWorkerQueue.Dequeue(OUT SendBuffer))
			{
				SendPacket(SendBuffer);
			}
		}
		
		// Sleep to prevent busy-waiting
		// FPlatformProcess::Sleep(0.01f);
		// but not critical for latency (is optional)
	}
	
	return 0;
}

void SendWorker::Exit()
{
	FRunnable::Exit();
}

void SendWorker::Destroy()
{
	Running = false;
}

bool SendWorker::SendPacket(TSharedPtr<SendBuffer> SendBuffer)
{
	if (not SendDesiredBytes(SendBuffer->Buffer(), SendBuffer->WriteSize()))
		return  false;
	
	return true;
}

bool SendWorker::SendDesiredBytes(const uint8* Buffer, int32 Size)
{
	while (Size > 0)
	{
		int32 BytesSend = 0;
		if (not Socket->Send(Buffer, Size, OUT BytesSend))
			return false;
		
		Size -= BytesSend;
		Buffer += BytesSend;
	}
	
	return true;
}
