#include "PacketSession.h"
#include "NetworkWorker.h"
#include "Generated/ClientPacketHandler.h"
#include "TTPacketHeader.h"

/*-----------------
   ClientSession
-----------------*/

PacketSession::PacketSession(class FSocket* Socket)
   : Socket(Socket)
{
   ClientPacketHandler::Init();
}

PacketSession::~PacketSession()
{
   Disconnect();
}

void PacketSession::Run()
{
   RecvWorkerThread = MakeShared<RecvWorker>(Socket, AsShared());
   SendWorkerThread = MakeShared<SendWorker>(Socket, AsShared());
}

void PacketSession::HandleRecvPackets()
{
   if (bDisconnected.Load())
      return;
   
   while (true)
   {
      TArray<uint8> Packet;
      if (not RecvPacketQueue.Dequeue(OUT Packet))
         break;
      
      const int32 OldSize = RecvStreamBuffer.Num();
      RecvStreamBuffer.AddUninitialized(Packet.Num());
      FMemory::Memcpy(RecvStreamBuffer.GetData() + OldSize, Packet.GetData(), Packet.Num());
   }
   
   TryAssemblePackets();
}

void PacketSession::SendPacket(TSharedPtr<SendBuffer> SendBuffer)
{
   SendPacketQueue.Enqueue(SendBuffer);
}

void PacketSession::Disconnect()
{
   if (bDisconnected.Exchange(true))
      return;

   if (RecvWorkerThread)
   {
      RecvWorkerThread->Destroy();
      RecvWorkerThread = nullptr;
   }

   if (SendWorkerThread)
   {
      SendWorkerThread->Destroy();
      SendWorkerThread = nullptr;
   }
}

void PacketSession::TryAssemblePackets()
{
   if (bDisconnected.Load())
      return;
   
   const int32 HeaderSize = sizeof(FPacketHeader);

   while (true)
   {
      if (RecvStreamBuffer.Num() < HeaderSize)
      {
         return;
      }

      FPacketHeader Header;
      {
         FMemoryReader Reader(RecvStreamBuffer);
         Reader << Header;
      }

      if (Header.packetSize < HeaderSize)
      {
         // 비정상 패킷
         Disconnect();
         // TODO: 여기서 Disconnect 하는 게 아닌 상위 객체 (NGIS)에 Disconnect 요청하기
         return;
      }

      if (RecvStreamBuffer.Num() < Header.packetSize)
      {
         // 아직 패킷 전체가 안 들어옴
         return;
      }

      TArray<uint8> CompletePacket;
      CompletePacket.Append(RecvStreamBuffer.GetData(), Header.packetSize);

      TSharedPtr<PacketSession> ThisPtr = AsShared();
      ClientPacketHandler::Dispatch(ThisPtr, CompletePacket.GetData(), CompletePacket.Num());

      // 앞에서 Header.PacketSize만큼 제거
      RecvStreamBuffer.RemoveAt(0, Header.packetSize, EAllowShrinking::No);
   }
}
