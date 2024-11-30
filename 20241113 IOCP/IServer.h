#pragma once

#include <Windows.h>
#include <stack>

class CPacket;
class CSession;

class CLanServer { 
public:
    virtual ~CLanServer() {}

    // ���� ����/����
    virtual bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount,
        bool nagleOption, int maxSessionCount) = 0;
    virtual void Stop() = 0;

    // ���� ����
    virtual int GetSessionCount() const = 0;    // ���� ������ ���ӵ� ������ ������ ��ȯ
    virtual bool Disconnect(UINT64 sessionID) = 0;  // ���������� ���� �����ϰ� netlib�� ������ �����ؾ��� �� ȣ���ϴ� �Լ�
    virtual void SendPacket(UINT64 sessionID, CPacket* packet) = 0; // Ư�� ���ǿ� ����ȭ ���۸� ���� �� ����ϴ� �Լ�

    virtual CSession* FetchSession(void) = 0;   // sessions���� ������ �ϳ� �������� �Լ�
    virtual void returnSession(CSession* pSession) = 0; // sessions�� ������ ��ȯ�ϴ� �Լ�
    virtual void InitSessionInfo(CSession* pSession) = 0;   // ������ �ʱ�ȭ �ϴ� �Լ�

    // �̺�Ʈ ó�� (���� ���� �Լ�)
    virtual bool OnConnectionRequest(const std::string& ip, int port) = 0;  // ó�� ��α� ť���� �����͸� ���� ���� �������� ���θ� �Ǵ��ϴ� �Լ�. ������ ������ �����, �ƴϸ� �״�� continue
    virtual void OnAccept(UINT64 sessionID) = 0;    // ������ �����ϰ� ������ �ʿ��� �ؾ��ϴ� ���� ������ �Լ�.
    virtual void OnRelease(UINT64 sessionID) = 0;   // ������ netlib�ʿ��� ���� ������ �� �������ʿ� �˸��� �Լ�. ���� Disconnect �Լ��� ���������� ���� �����ٸ� continue
    virtual void OnRecv(UINT64 sessionID, CPacket* packet) = 0; // netlib���� ��Ŷ�� �޾��� �� ��Ŷ�� ó���� ���� ȣ���ϴ� �Լ�. ������ �ʿ��� �����Ѵ�� ��Ŷ�� ó���Ѵ�.
    virtual void OnError(int errorCode, const wchar_t* errorMessage) = 0;   // ������ ���� ��� �α׸� ����� ���� �Լ�. ���߿� ����, �񵿱� �����Լ� 2���� ���� ����

    // recv, send �� ������ �ϴ� �Լ�
    void RecvPost(CSession* pSession);
    void SendPost(CSession* pSession);

    // ���Ʈ/��Ŀ ������, static �Լ��̱� ������ ���ڷ� ��ü�� �������. �Լ� ��ü�� ������ Ʋ�� ���� ��.
    static unsigned int WINAPI WorkerThread(void* pArg);
    static unsigned int WINAPI AcceptThread(void* pArg);

    // ����͸� ����
    UINT32 GetAcceptTPS(void) const { return acceptTPS; }
    UINT32 GetRecvMessageTPS(void) const { return recvMessageTPS; }
    UINT32 GetSendMessageTPS(void) const { return sendMessageTPS; }

    void ClearTPSValue(void);   // TPS �������� �������� ��� 0���� �ʱ�ȭ �ϴ� �Լ�

    UINT32 GetDisconnectedSessionCnt(void) const { return disconnectedSessionCnt; }

private:
    // �� �ൿ�� �ʴ� �󸶳� �Ͼ���� Ȯ���ϴ� ���� 
    UINT32 acceptTPS = 0;
    UINT32 recvMessageTPS = 0;
    UINT32 sendMessageTPS = 0;

protected:
    HANDLE hIOCP;  // IOCP �ڵ�
    SOCKET listenSocket; // listen ����
    CSession* sessions;  // ���� ����
    
    UINT8 threadCnt;    // ������ ������ ����
    HANDLE* hThreads;   // ���� �������� �ڵ鰪. �� ������ ���� ����� �������� ���� ���θ� �Ǵ���. WaitForMultipleObjects ȣ�⿡ ���.

protected:
    std::stack<UINT16> stSessionIndex; // ������ �ε������� �����ϴ� ����, ���ٽ� ����ȭ lock�� �ɰ� ���
    CRITICAL_SECTION cs_sessionID;

protected:
    UINT64 uniqueSessionID = 0; // ������ ������ �� ���� 1�� �����ϴ� ����
    UINT16 maxConnections = 0;  // �ִ� ���� ���� ��

protected:
    UINT32 curSessionCnt = 0;   // ���� �������� ���� ��
    UINT32 disconnectedSessionCnt = 0;  // �߰��� ������ ���Ǽ�
};