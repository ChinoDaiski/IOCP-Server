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
    bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, int maxSessionCount) override;
    void Stop(void) override;

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
    void InitWinSock(void) noexcept;
    void CreateListenSocket(PROTOCOL_TYPE type);
    void CreateIOCP(int runningThreadCount);
    void Bind(unsigned long ip, UINT16 port);
    void SetOptions(bool bNagleOn);
    void Listen(void);

private:
    void InitResource(int maxSessionCount);
    void ReleaseResource(void);

    void CreateThreadPool(int workerThreadCount);

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

    std::unordered_set<std::string> allowedIPs; // ����� IP ���
    std::unordered_set<std::string> blockedIPs; // ������ IP ���
    


    // ���� ���˸������ Ȯ���ϴ� ����
    bool isServerMaintrenanceMode = false;
};
