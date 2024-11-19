#include "ServerLib.h"

#include "Session.h"

ServerLib::ServerLib(void)
	: g_ID{ 0 }
{
}

void ServerLib::SendPacket(int sessionID, CPacket* pPacket)
{
	// ���� �˻�
	auto iter = sessionMap.find(sessionID);

	// ������ ã�Ҵٸ�
	if (iter != sessionMap.end())
	{
		iter->second->sendQ.Enqueue(pPacket->GetBufferPtr(), pPacket->GetBufferSize());
		pPacket->Clear();
	}
}

void ServerLib::RegisterSession(CSession* _pSession)
{
	sessionMap.emplace(g_ID, _pSession);
	++g_ID;
}
