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

class CGameServer : public CLanServer {
public:
    bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, UINT16 maxSessionCount) override;

    void Stop() override {
        // 모든 스레드 종료 및 리소스 정리
    }

    int GetSessionCount() const override {
        return static_cast<int>(sessions.size());
    }

    // 컨텐츠에서 netlib에게 세션을 종료하라고 알려주는 함수
    bool Disconnect(UINT32 sessionID) override;

    // 패킷 송신 처리
    bool SendPacket(UINT32 sessionID, CPacket* packet) override;

    // 이벤트 가상 함수 기본 구현
    bool OnConnectionRequest(const std::string& ip, int port) override;
    void OnAccept(UINT32 sessionID) override;
    void OnRelease(UINT32 sessionID) override;
    void OnRecv(UINT32 sessionID, CPacket* packet) override;
    void OnError(int errorCode, const wchar_t* errorMessage) override {}

    int GetAcceptTPS() const override { return acceptTPS; }
    int GetRecvMessageTPS() const override { return recvMessageTPS; }
    int GetSendMessageTPS() const override { return sendMessageTPS; }



private:
    virtual CSession* FetchSession(void) override;
    virtual void returnSession(CSession* pSession) override;
    virtual CSession* InitSessionInfo(CSession* pSession) override;

    UINT64 uniqueSessionID = 0; // 세션이 접속할 때 마다 1씩 증가하는 변수
    UINT16 maxConnections = 0;  // 최대 동접 세션 수


    std::stack<UINT16> stSessionIndex; // 
    CRITICAL_SECTION cs_sessionID;






    void loadIPList(const std::string& filePath, std::unordered_set<std::string>& IPList);

    std::unordered_set<std::string> allowedIPs; // 허용할 IP 목록
    std::unordered_set<std::string> blockedIPs; // 차단할 IP 목록
    

private:
    int acceptTPS = 0;
    int recvMessageTPS = 0;
    int sendMessageTPS = 0;

    bool isServerMaintrenanceMode = false;
};
