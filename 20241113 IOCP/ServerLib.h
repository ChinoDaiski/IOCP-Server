#pragma once

#include "Packet.h"
#include "IServer.h"
#include "IContent.h"

#include <unordered_map>

class CSession;

class ServerLib : public IServer
{
public:
    ServerLib(void);
    ~ServerLib(void) = default;

public:
    void RegisterIContent(IContent* _pIContent) { pIContent = _pIContent; }

public:
    // 콘텐츠에서 보낼 것이 생겼을 때 호출되는 함수
    virtual void SendPacket(int sessionID, CPacket* pPacket);

    void RegisterSession(CSession* _pSession);

private:
    std::unordered_map<UINT32, CSession*> sessionMap;
    UINT32 g_ID;

private:
    IContent* pIContent;
};
