
#include "Content.h"

UINT32 CPlayer::g_ID = 0;

void ServerContent::OnRecv(int sessionID, CPacket* pPacket)
{
	pIServer->SendPacket(sessionID, pPacket);

	// 나중엔 검색이 들어감. 지금은 echo니깐 위 처럼 바로 sendPacket 호출
	//// 세션 검색
	//auto iter = playerMap.find(sessionID);

	//// 세션을 찾았다면
	//if (iter != playerMap.end())
	//{
	//	pIServer->SendPacket(sessionID, pPacket);
	//}
}

void ServerContent::OnAccept(int sessionID)
{
	CPlayer* pPlayer = new CPlayer;
	pPlayer->sessionID = sessionID;

	// 원래는 플레이어 ID 따로 관리해야하는데, 일단 ECHO 서버니깐 세션 ID를 key값으로 사용
	playerMap.emplace(sessionID, pPlayer);
}

CPlayer::CPlayer()
{
	playerID = g_ID;
	g_ID++;
}

CPlayer::~CPlayer()
{
}
