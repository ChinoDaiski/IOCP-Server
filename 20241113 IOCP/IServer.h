#pragma once

#include <Windows.h>
#include <unordered_map>

class CPacket;
class CSession;

class IServer
{
public:
    virtual ~IServer() = default;

    // �������� ȣ���ϴ� �Լ�
public:
    // �������� ���� ���� ������ �� ȣ��Ǵ� �Լ�
    virtual void SendPacket(UINT64 sessionID, CPacket* pPacket) = 0;
};


class CLanServer { 
public:
    virtual ~CLanServer() {}

    // ���� ����/����
    virtual bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount,
        bool nagleOption, UINT16 maxSessionCount) = 0;
    virtual void Stop() = 0;

    // ���� ����
    virtual int GetSessionCount() const = 0;
    virtual bool Disconnect(UINT32 sessionID) = 0;
    virtual bool SendPacket(UINT32 sessionID, CPacket* packet) = 0;


    virtual CSession* FetchSession(void) = 0;   // sessions���� ������ �ϳ� �������� �Լ�
    virtual void returnSession(CSession* pSession) = 0; // sessions�� ������ ��ȯ�ϴ� �Լ�
    virtual CSession* InitSessionInfo(CSession* pSession) = 0;



    // �̺�Ʈ ó�� (���� ���� �Լ�)
    virtual bool OnConnectionRequest(const std::string& ip, int port) = 0;
    virtual void OnAccept(UINT32 sessionID) = 0;
    virtual void OnRelease(UINT32 sessionID) = 0;
    virtual void OnRecv(UINT32 sessionID, CPacket* packet) = 0;
    virtual void OnError(int errorCode, const wchar_t* errorMessage) = 0;

    // ����͸�
    virtual int GetAcceptTPS() const = 0;
    virtual int GetRecvMessageTPS() const = 0;
    virtual int GetSendMessageTPS() const = 0;

    void RecvPost(CSession* pSession);
    void SendPost(CSession* pSession);

    // ��Ŀ ������, ���ڷ� 
    static unsigned int WINAPI WorkerThread(void* pArg);
    static unsigned int WINAPI AcceptThread(void* pArg);

protected:
    HANDLE hIOCP;  // IOCP �ڵ�
    SOCKET listenSocket; // listen ����
    CSession* sessions;  // ���� ����
    
    UINT8 threadCnt;    // ������ ������ ����
    HANDLE* hThreads;
};