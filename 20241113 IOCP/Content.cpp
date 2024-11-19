
#include "pch.h"

#include "Content.h"
#include "Session.h"

UINT32 CPlayer::g_ID = 0;

void ServerContent::OnRecv(int sessionID, CPacket* pPacket)
{
	// ���⼭ ������ ó���� ��.
	// ������ ó���� �ϸ鼭 sendQ�� �����͸� ����. SendPost �Լ� ȣ��� sendQ�� ���� �����͸� ���� �� �ִٸ� ������.
	pIServer->SendPacket(sessionID, pPacket);

	// ���߿� �˻��� ��. ������ echo�ϱ� �� ó�� �ٷ� sendPacket ȣ��
	//// ���� �˻�
	//auto iter = playerMap.find(sessionID);

	//// ������ ã�Ҵٸ�
	//if (iter != playerMap.end())
	//{
	//	pIServer->SendPacket(sessionID, pPacket);
	//}
}

void ServerContent::OnAccept(int sessionID)
{
	CPlayer* pPlayer = new CPlayer;
	pPlayer->sessionID = sessionID;

	// ������ �÷��̾� ID ���� �����ؾ��ϴµ�, �ϴ� ECHO �����ϱ� ���� ID�� key������ ���
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
