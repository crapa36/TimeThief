#include "ClientSession.h"
#include "NetworkWorker.h"
#include <google/protobuf/message.h>

/*-----------------
   ClientSession
-----------------*/

ClientSession::ClientSession(class FSocket* Socket)
{
   // TODO: PacketHandler가 PacketHandler 만들어 진 뒤 초기화 하기
}

ClientSession::~ClientSession()
{
   Disconnect();
}

void ClientSession::Run()
{
   RecvWorkerThread = MakeShared<RecvWorker>(Socket, AsShared());
   SendWorkerThread = MakeShared<SendWorker>(Socket, AsShared());
}

void ClientSession::HandleRecvPackets()
{
   google::protobuf::Message* Message = nullptr;
   
   while (true)
   {
      TArray<uint8> Packet;
      if (not RecvPacketQueue.Dequeue(OUT Packet))
         break;
      
      TSharedPtr<ClientSession> ThisPtr = AsShared();
      // TODO: PacketHandler가 완성되고 패킷을 조립하고 구분하는 작업 수행 시키기
      //       ex) PacketHandler::HandlePacket(ThisPtr, Packet.GetData(), Packet.Num());
   }
}

void ClientSession::SendPacket(TSharedPtr<SendBuffer> SendBuffer)
{
   SendPacketQueue.Enqueue(SendBuffer);
}

void ClientSession::Disconnect()
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
