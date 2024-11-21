
#include "pch.h"

#include "Content.h"
#include "Session.h"

UINT32 CPlayer::g_ID = 0;

void ServerContent::OnRecv(int sessionID, CPacket* pPacket)
{
	// 여기서 컨텐츠 처리가 들어감.
	// ~ 컨텐츠 ~
	// 컨텐츠 처리를 하면서 패킷을 만들어 SendPacket를 호출
	// sendQ에 데이터를 삽입. + sending 여부를 판단해 sendQ에 넣은 데이터를 보낼 수 있다면 보낸다.
	pIServer->SendPacket(sessionID, pPacket);
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
