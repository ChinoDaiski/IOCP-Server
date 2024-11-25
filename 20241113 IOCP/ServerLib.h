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
    // 콘텐츠에서 보낼 것이 생겼을 때 호출되는 함수
    virtual void SendPacket(UINT64 sessionID, CPacket* pPacket) override;

public:
    CSession* FetchSession(void);
    void DeleteSession(CSession* _pSession);
    void SendPost(CSession* pSession);
    void RecvPost(CSession* pSession);

private:
    CSession sessionArr[MAX_SESSION_CNT];
    UINT32 g_ID;    // 세션이 접속할 때 마다 1씩 증가.

    CircularQueue<std::pair<UINT64, UINT16>> debugSessionIndexQueue;

private:
    IContent* pIContent;
};
