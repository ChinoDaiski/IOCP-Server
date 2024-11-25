#pragma once

#include "Packet.h"
#include "IServer.h"
#include "IContent.h"

#include <unordered_map>

class CPlayer
{
public:
    CPlayer();
    ~CPlayer();

public:
    UINT32 sessionID;
    UINT32 playerID;

private:
    static UINT32 g_ID;
};

class ServerContent : public IContent
{
public:
    ~ServerContent(void) = default;

public:
    void RegisterIServer(IServer* _pIServer) { pIServer = _pIServer; }

public:
    // Ŭ���̾�Ʈ�κ��� �����͸� �������� �� ȣ��Ǵ� �Լ�. Ŭ���̾�Ʈ���� ����
    virtual void OnRecv(UINT64 sessionID, CPacket* pPacket) override;

    // ������ ������ �ް�, ���ǰ� ����� ��ü�� ����� ���� ���������� ȣ���ؾ��ϴ� �Լ�
    virtual void OnAccept(UINT64 sessionID) override;

private:
    std::unordered_map<UINT32, CPlayer*> playerMap;

private:
    IServer* pIServer;
};