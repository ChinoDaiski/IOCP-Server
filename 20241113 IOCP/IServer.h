#pragma once

#include <Windows.h>
#include <stack>
#include "concurrent_stack.h"

class CPacket;
class CSession;

// �� �ൿ�� �ʴ� �󸶳� �Ͼ���� Ȯ���ϴ� ���� 
struct FrameStats
{
    UINT32 accept;
    UINT32 recvMessage;
    UINT32 sendMessage;

    // �޸� Ǯ ����. ���� index ���� �����ϴ� concurrent_stack�� ������ �ִ� �޸� Ǯ�� ��뷮.
    UINT32 CurPoolCount;
    UINT32 MaxPoolCount;
};

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
    const FrameStats& GetStatusTPS(void) const { return statusTPS; }
    void ClearTPSValue(void);   // TPS �������� �������� ��� 0���� �ʱ�ȭ �ϸ鼭 statusTPS�� ���� �ִ� �Լ�.

    UINT32 GetDisconnectedSessionCnt(void) const { return disconnectedSessionCnt; }

private:
    // �� �ൿ�� �ʴ� �󸶳� �Ͼ���� Ȯ���ϴ� ���� 
    UINT32 acceptTPS = 0;
    UINT32 recvMessageTPS = 0;
    UINT32 sendMessageTPS = 0;

    FrameStats statusTPS;

protected:
    HANDLE hIOCP;  // IOCP �ڵ�
    SOCKET listenSocket; // listen ����
    CSession* sessions;  // ���� ����
    
    UINT8 threadCnt;    // ������ ������ ����
    HANDLE* hThreads;   // ���� �������� �ڵ鰪. �� ������ ���� ����� �������� ���� ���θ� �Ǵ���. WaitForMultipleObjects ȣ�⿡ ���.

protected:
    concurrent_stack<UINT16> stSessionIndex; // ������ �ε������� �����ϴ� ����
    CRITICAL_SECTION cs_sessionID;

protected:
    UINT64 uniqueSessionID = 0; // ������ ������ �� ���� 1�� �����ϴ� ����
    UINT16 maxConnections = 0;  // �ִ� ���� ���� ��

protected:
    UINT32 curSessionCnt = 0;   // ���� �������� ���� ��
    UINT32 disconnectedSessionCnt = 0;  // �߰��� ������ ���Ǽ�
};