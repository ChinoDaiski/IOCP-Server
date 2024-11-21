
#include "pch.h"

#include "Content.h"
#include "Session.h"

UINT32 CPlayer::g_ID = 0;

void ServerContent::OnRecv(int sessionID, CPacket* pPacket)
{
	// ���⼭ ������ ó���� ��.
	// ~ ������ ~
	// ������ ó���� �ϸ鼭 ��Ŷ�� ����� SendPacket�� ȣ��
	// sendQ�� �����͸� ����. + sending ���θ� �Ǵ��� sendQ�� ���� �����͸� ���� �� �ִٸ� ������.
	pIServer->SendPacket(sessionID, pPacket);
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
