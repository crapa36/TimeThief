#include "NetworkWorker.h"

/*--------------
   RecvWorker
--------------*/

RecvWorker::RecvWorker(FSocket* Socket, TSharedPtr<PacketSession> Session)
	: Socket(Socket), SessionRef(Session)
{
	Thread = FRunnableThread::Create(this, TEXT("RecvWorkerThread"));
}

RecvWorker::~RecvWorker()
{
}

bool RecvWorker::Init()
{
	return FRunnable::Init();
}

uint32 RecvWorker::Run()
{
	while (Running)
	{
		if (!PumpRecv())
		{
			FPlatformProcess::Sleep(0.001f);
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
	
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool RecvWorker::PumpRecv()
{
	if (!Socket || !Running)
		return false;

	uint32 PendingDataSize = 0;
	if (!Socket->HasPendingData(PendingDataSize) || PendingDataSize == 0)
		return false;

	const int32 ReadSize = FMath::Min<int32>((int32)PendingDataSize, 4096);

	TArray<uint8> TempBuffer;
	TempBuffer.SetNumUninitialized(ReadSize);

	int32 NumRead = 0;
	if (!Socket->Recv(TempBuffer.GetData(), ReadSize, NumRead))
	{
		Running = false;
		return false;
	}

	if (NumRead <= 0)
	{
		Running = false;
		return false;
	}

	if (TSharedPtr<PacketSession> Session = SessionRef.Pin())
	{
		Session->RecvPacketQueue.Enqueue(MoveTemp(TempBuffer));
		return true;
	}

	Running = false;
	return false;
}

// bool RecvWorker::ReceivePacket(TArray<uint8>& OutPacket)
// {
// 	// TEMP: PacketHeader Size는 FTempHeader size라고 정의
// 	const int32 HeaderSize = sizeof(FTempHeader);
// 	TArray<uint8> HeaderBuffer;
// 	HeaderBuffer.AddZeroed(HeaderSize);
// 	
// 	if (not ReceiveDesiredBytes(HeaderBuffer.GetData(), HeaderSize))
// 		return false;
// 	
// 	FTempHeader Header;
// 	{
// 		FMemoryReader Reader(HeaderBuffer);
// 		Reader << Header;
// 		// UE_LOG(LogTemp, Log, TEXT("PacketId: %d, PacketSize: %d"), Header.PacketId, Header.PacketSize);
// 	}
// 	
// 	OutPacket = HeaderBuffer;
// 	
// 	TArray<uint8> PayloadBuffer;
// 	const int32 PayloadSize = Header.PacketSize - HeaderSize;
// 	if (PayloadSize == 0)
// 		return true;
// 	
// 	OutPacket.AddZeroed(PayloadSize);
// 	
// 	if (ReceiveDesiredBytes(&OutPacket[HeaderSize], PayloadSize))
// 		return true;
// 	
// 	return false;
// }
//
// bool RecvWorker::ReceiveDesiredBytes(uint8* Result, int32 Size)
// {
// 	if (!Socket || !Running)
// 		return false;
// 	
// 	uint32 PendingDataSize = 0;
// 	if (Socket->HasPendingData(PendingDataSize) == false or PendingDataSize <= 0)
// 		return false;
// 	
// 	int32 Offset = 0;
//
// 	while (Size > 0)
// 	{
// 		int32 NumRead = 0;
// 		Socket->Recv(Result + Offset, Size, OUT NumRead);
// 		check(NumRead <= Size);
// 		
// 		if (NumRead <= 0)
// 			return false;
// 		
// 		Offset += NumRead;
// 		Size -= NumRead;
// 	}
// 	
// 	return true;
// }

/*--------------
   SendWorker
--------------*/

SendWorker::SendWorker(FSocket* Socket, TSharedPtr<PacketSession> Session)
	: Socket(Socket), SessionRef(Session)
{
	Thread = FRunnableThread::Create(this, TEXT("SendWorkerThread"));
}

SendWorker::~SendWorker()
{
}

bool SendWorker::Init()
{
	return FRunnable::Init();
}

uint32 SendWorker::Run()
{
	while (Running)
	{
		TSharedPtr<SendBuffer> SendBuffer;

		if (TSharedPtr<PacketSession> Session = SessionRef.Pin())
		{
			if (Session->SendPacketQueue.Dequeue(SendBuffer))
			{
				if (!SendPacket(SendBuffer))
				{
					break;
				}
			}
			else
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}
		else
		{
			break;
		}
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
	
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool SendWorker::SendPacket(TSharedPtr<SendBuffer> SendBuffer)
{
	if (not SendDesiredBytes(SendBuffer->Buffer(), SendBuffer->WriteSize()))
		return  false;
	
	return true;
}

bool SendWorker::SendDesiredBytes(const uint8* Buffer, int32 Size)
{
	if (!Socket || !Running)
		return false;
	
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
