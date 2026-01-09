#include "ClientSession.h"

/*-----------------
   ClientSession
-----------------*/

ClientSession::ClientSession(class FSocket* Socket)
{
}

ClientSession::~ClientSession()
{
}

void ClientSession::Run()
{
}

void ClientSession::HandleRecvPackets()
{
}

void ClientSession::SendPacket(TSharedPtr<SendBuffer> SendBuffer)
{
}

void ClientSession::Disconnect()
{
}
