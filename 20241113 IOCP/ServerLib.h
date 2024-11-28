#pragma once

#include "Packet.h"
#include "IServer.h"

#define MAX_SESSION_CNT 100

class CGameServer : public CLanServer {
public:
    bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, UINT16 maxSessionCount) override;
    void Stop(void) override;

    void Release(void);

    // 해당 값을 그냥 가져오기에 정확하지 않음. 감시용도로 대략적인 측정시 사용
    int GetSessionCount() const override { return static_cast<int>(curSessionCnt); }

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
