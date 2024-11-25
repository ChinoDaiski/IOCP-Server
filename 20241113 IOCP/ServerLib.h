#pragma once

#include "Packet.h"
#include "IServer.h"
#include "IContent.h"

#include <stack>

#define MAX_SESSION_CNT 100

//class CSession;

#include "Session.h"

class ServerLib : public IServer
{
public:
    ServerLib(void);
    ~ServerLib(void);

public:
    void RegisterIContent(IContent* _pIContent) { pIContent = _pIContent; }

public:
    // ���������� ���� ���� ������ �� ȣ��Ǵ� �Լ�
    virtual void SendPacket(UINT64 sessionID, CPacket* pPacket) override;

public:
    CSession* FetchSession(void);
    void DeleteSession(CSession* _pSession);
    void SendPost(CSession* pSession);
    void RecvPost(CSession* pSession);

private:
    CSession sessionArr[MAX_SESSION_CNT];
    UINT32 g_ID;    // ������ ������ �� ���� 1�� ����.

    CircularQueue<std::pair<UINT64, UINT16>> debugSessionIndexQueue;

private:
    IContent* pIContent;
};
