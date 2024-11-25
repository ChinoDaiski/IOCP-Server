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
    // 클라이언트로부터 데이터를 수신했을 때 호출되는 함수. 클라이언트에서 정의
    virtual void OnRecv(UINT64 sessionID, CPacket* pPacket) override;

    // 서버가 세션을 받고, 세션과 연결된 객체를 만들기 위해 컨텐츠에서 호출해야하는 함수
    virtual void OnAccept(UINT64 sessionID) override;

private:
    std::unordered_map<UINT32, CPlayer*> playerMap;

private:
    IServer* pIServer;
};