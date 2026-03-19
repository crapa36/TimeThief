#include "PacketSession.h"
#include "NetworkWorker.h"
#include "Generated/ClientPacketHandler.h"

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
   while (true)
   {
      TArray<uint8> Packet;
      if (not RecvPacketQueue.Dequeue(OUT Packet))
         break;
      
      TSharedPtr<PacketSession> ThisPtr = AsShared();
      ClientPacketHandler::Dispatch(ThisPtr, Packet.GetData(), Packet.Num());
   }
}

void PacketSession::SendPacket(TSharedPtr<SendBuffer> SendBuffer)
{
   SendPacketQueue.Enqueue(SendBuffer);
}

void PacketSession::Disconnect()
{
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
