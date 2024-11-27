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

class CGameServer : public CLanServer {
public:
    bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, UINT16 maxSessionCount) override;

    void Stop() override {
        // ��� ������ ���� �� ���ҽ� ����
    }

    int GetSessionCount() const override {
        return static_cast<int>(sessions.size());
    }

    // ���������� netlib���� ������ �����϶�� �˷��ִ� �Լ�
    bool Disconnect(UINT32 sessionID) override;

    // ��Ŷ �۽� ó��
    bool SendPacket(UINT32 sessionID, CPacket* packet) override;

    // �̺�Ʈ ���� �Լ� �⺻ ����
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

    UINT64 uniqueSessionID = 0; // ������ ������ �� ���� 1�� �����ϴ� ����
    UINT16 maxConnections = 0;  // �ִ� ���� ���� ��


    std::stack<UINT16> stSessionIndex; // 
    CRITICAL_SECTION cs_sessionID;






    void loadIPList(const std::string& filePath, std::unordered_set<std::string>& IPList);

    std::unordered_set<std::string> allowedIPs; // ����� IP ���
    std::unordered_set<std::string> blockedIPs; // ������ IP ���
    

private:
    int acceptTPS = 0;
    int recvMessageTPS = 0;
    int sendMessageTPS = 0;

    bool isServerMaintrenanceMode = false;
};
