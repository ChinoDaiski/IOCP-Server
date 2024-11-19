
#include "pch.h"

#include "Content.h"
#include "Session.h"

UINT32 CPlayer::g_ID = 0;

void ServerContent::OnRecv(int sessionID, CPacket* pPacket)
{
	// 여기서 컨텐츠 처리가 들어감.
	// 컨텐츠 처리를 하면서 sendQ에 데이터를 삽입. SendPost 함수 호출시 sendQ에 넣은 데이터를 보낼 수 있다면 보낸다.
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
