#pragma once

#include "Packet.h"
#include "IServer.h"

#define MAX_SESSION_CNT 100

class CGameServer : public CLanServer {
public:
    bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, UINT16 maxSessionCount) override;
    void Stop(void) override;

    void Release(void);

    // �ش� ���� �׳� �������⿡ ��Ȯ���� ����. ���ÿ뵵�� �뷫���� ������ ���
    int GetSessionCount() const override { return static_cast<int>(curSessionCnt); }

    // ���������� netlib���� ������ �����϶�� �˷��ִ� �Լ�
    bool Disconnect(UINT64 sessionID) override;

    // ��Ŷ �۽� ó��
    void SendPacket(UINT64 sessionID, CPacket* packet) override;

    // �̺�Ʈ ���� �Լ� �⺻ ����
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

    std::unordered_set<std::string> allowedIPs; // ����� IP ���
    std::unordered_set<std::string> blockedIPs; // ������ IP ���
    


    // ���� ���˸������ Ȯ���ϴ� ����
    bool isServerMaintrenanceMode = false;
};
