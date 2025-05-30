#pragma once

#include "IServer.h"

#define MAX_SESSION_CNT 100

#define OPTION_NONBLOCKING      0x01
#define OPTION_DISABLE_NAGLE    0x02
#define OPTION_ENABLE_RST       0x04

enum class PROTOCOL_TYPE
{
    TCP_IP,
    UDP,
    END
};

class CSession;
class CPacket;
class CLanServer;

class CGameServer : public CLanServer {
public:
    bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, int maxSessionCount, int pendingAcceptCount) override;
    void Stop(void) override;

    // 컨텐츠에서 netlib에게 세션을 종료하라고 알려주는 함수
    bool Disconnect(UINT64 sessionID) override;

    // 패킷 송신 처리
    void SendPacket(UINT64 sessionID, CPacket* packet) override;

    // 이벤트 가상 함수 기본 구현
    bool OnConnectionRequest(const std::string& ip, int port) override;
    void OnAccept(UINT64 sessionID) override;
    void OnRelease(UINT64 sessionID) override;
    void OnRecv(UINT64 sessionID, CPacket* packet) override;
    void OnError(int errorCode, const wchar_t* errorMessage) override {}

private:
    void InitWinSock(void) noexcept;
    void CreateListenSocket(PROTOCOL_TYPE type);
    void CreateIOCP(int runningThreadCount);
    void Bind(unsigned long ip, UINT16 port);
    void SetOptions(bool bNagleOn);
    void Listen(void);

private:
    void InitResource(int maxSessionCount);
    void ReleaseResource(void);

private:
    void CreateThreadPool(int workerThreadCount);

private:
    void ReserveAcceptSocket(int pendingAcceptCount);

private:
    void SetTCPSendBufSize(SOCKET socket, UINT32 size);
    void SetNonBlockingMode(SOCKET socket, bool bNonBlocking);
    void DisableNagleAlgorithm(SOCKET socket, bool bNagleOn);
    void EnableRSTOnClose(SOCKET socket);

private:
    virtual CSession* FetchSession(void) override;
    virtual void returnSession(CSession* pSession) override;
    virtual void InitSessionInfo(CSession* pSession) override;

private:
    void loadIPList(const std::string& filePath, std::unordered_set<std::string>& IPList);

    std::unordered_set<std::string> allowedIPs; // 허용할 IP 목록
    std::unordered_set<std::string> blockedIPs; // 차단할 IP 목록
    
    // 서버 정검모드인지 확인하는 변수
    bool isServerMaintrenanceMode = false;
};
