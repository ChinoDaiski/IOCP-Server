#define _WINSOCK_DEPRECATED_NO_WARNINGS // �ֽ� VC++ ������ �� ��� ����



#include <iostream>
#define NOMINMAX
#include <WinSock2.h>
#pragma comment(lib, "ws2_32")

#include "RingBuffer.h"

#include <process.h>

#include <unordered_map>

#include "Packet.h"


#pragma pack(push, 1)   

typedef struct _tagPACKET_HEADER {
    UINT16 byLen;   // ��Ŷ ����
}PACKET_HEADER;

#pragma pack(pop)

#define dfNETWORK_PACKET_CODE 0x89
#define dfMAX_PACKET_SIZE	10


#define SERVERPORT 6000


// ���� �迭
//��������� �����ϴ� ���� ī���͸� �ϳ� �ּ�
//
//��������� ���� ���� ī���͸� interlock���� 1�� ������Ű�鼭 �迭���� index�� ����
//
//ī���Ͷ� index�� �Ѵ� ���?



// ���� ���� ������ ���� ����ü
class CSession
{
public:
    CSession(){
        InitializeCriticalSection(&cs);
        bSending = false;
    }
    ~CSession(){
        DeleteCriticalSection(&cs);
    }

public:
    CRITICAL_SECTION cs;

    SOCKET sock;

    USHORT port;
    char IP[16];

    OVERLAPPED overlappedRecv;
    OVERLAPPED overlappedSend;

    CRingBuffer recvBuffer;
    CRingBuffer sendBuffer;

    WSABUF wsaRecvBuf[2];
    WSABUF wsaSendBuf[2];

    UINT32 IOcount;

    bool bSending;

    UINT32 iSessionID;
};

class CPlayer
{
public:
    CPlayer() {}
    ~CPlayer() {}

public:
    UINT32 m_playerID;
};



HANDLE g_hIOCP;
SOCKET g_listen_sock;
UINT32 g_SessionID = 0;


std::unordered_map<UINT32, CSession*> g_sessionMap; // key�� SessionID, value�� ���� �����͸� �޾� �����ϴ� ���� ���Ǹ�
std::unordered_map<UINT32, CPlayer*> g_playerMap; // key�� SessionID, value�� ���� �����͸� �޾� �����ϴ� ���� ���Ǹ�

CRITICAL_SECTION g_sessionMap_cs;


// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(LPVOID arg);
unsigned int WINAPI AcceptThread(void* pArg);


void RecvPost(CSession* pSession);
void SendPost(CSession* pSession);
void SendPacket(int playerID, CPacket* pPacket);
void OnRecv(int sessionID, CPacket* pPacket);
bool ProcessMessage(CSession* pSession);


// �������� �̺�Ʈ ����
HANDLE g_hExitThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

int main(void)
{
    int retval;

    // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // CPU ���� Ȯ��
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int ProcessorNum = si.dwNumberOfProcessors;

    // ����� �Ϸ� ��Ʈ ����
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
    if (g_hIOCP == NULL) return 1;

    // socket()
    g_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_sock == INVALID_SOCKET)
        std::cout << "INVALID_SOCKET : listen_sock\n";


    // recvbuf ũ�� ���� (0���� ����)
    int recvBufSize = 0;
    if (setsockopt(g_listen_sock, SOL_SOCKET, SO_RCVBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << std::endl;
        closesocket(g_listen_sock);
        WSACleanup();
        return 1;
    }


    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = bind(g_listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        std::cout << "bind()\n";

    // listen()
    retval = listen(g_listen_sock, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        std::cout << "listen()\n";








    InitializeCriticalSection(&g_sessionMap_cs);






    HANDLE hAcceptThread;		// ���ӿ�û, ���⿡ ���� ó��	
    HANDLE hWorkerThread;		// ��Ŀ ������

    DWORD dwThreadID;

    hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);








    //WCHAR ControlKey;

    //------------------------------------------------
    // ���� ��Ʈ��...
    //------------------------------------------------
    while (1)
    {
        //ControlKey = _getwch();
        //if (ControlKey == L'q' || ControlKey == L'Q')
        //{
        //    //------------------------------------------------
        //    // ����ó��
        //    //------------------------------------------------

        //    SetEvent(g_hExitThreadEvent);
        //    break;
        //}
    }











    ////------------------------------------------------
    //// ������ ���� ���
    ////------------------------------------------------
    //HANDLE hThread[] = { hAcceptThread, hWorkerThread};
    //DWORD retVal = WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
    //DWORD retError = WSAGetLastError();





    DeleteCriticalSection(&g_sessionMap_cs);






    ////------------------------------------------------
    //// ������ �ڵ�  ������ �������� Ȯ��.
    ////------------------------------------------------
    //DWORD ExitCode;

    //wprintf(L"\n\n--- THREAD CHECK LOG -----------------------------\n\n");

    //GetExitCodeThread(hAcceptThread, &ExitCode);
    //if (ExitCode != 0)
    //    wprintf(L"error - Accept Thread not exit\n");

    //GetExitCodeThread(hWorkerThread, &ExitCode);
    //if (ExitCode != 0)
    //    wprintf(L"error - Accept Thread not exit\n");


    // ���� ����
    WSACleanup();
    return 0;
}


// recv �����ۿ� �ִ� �����͸� ��Ŷ ������ ������
bool ProcessMessage(CSession* pSession)
{
    // 1. RecvQ�� �ּ����� ����� �ִ��� Ȯ��. ������ [ ��� ������ �̻��� �����Ͱ� �ִ��� Ȯ�� ]�ϴ� ��.
    if (pSession->recvBuffer.GetUseSize() < 10)
        return false;

    //// 2. RecvQ���� PACKET_HEADER ���� Peek
    //PACKET_HEADER header;
    //int headerSize = sizeof(header);
    //
    //int retVal = pSession->recvBuffer.Peek(reinterpret_cast<char*>(&header), headerSize);

    //// 4. ����� len���� RecvQ�� ������ ������ ��
    //if (sizeof(PACKET_HEADER) > pSession->recvBuffer.GetUseSize())
    //    return false;

    // 5. Peek �ߴ� ����� RecvQ���� �����.
    //pSession->recvBuffer.MoveFront(sizeof(PACKET_HEADER));

    // 6. RecvQ���� header�� len ũ�⸸ŭ �ӽ� ��Ŷ ���۸� �̴´�.
    CPacket Packet;
    int recvQDeqRetVal = pSession->recvBuffer.Dequeue(Packet.GetBufferPtr(), 10);
    Packet.MoveWritePos(10);

    if (recvQDeqRetVal != 10)
    {
        DebugBreak();
    }

    // OnRecv ȣ��
    OnRecv(pSession->iSessionID, &Packet);

    return true;
}


// ������ �Լ�
void OnRecv(int sessionID, CPacket* pPacket)
{
    
    // ���⼭ ������ ó��
    /*
        (*iter).second �� �÷��̾��̸� ��Ŷ�� Ÿ�Կ� ���� �÷��̾� ����
    */

    // �߰��߰� send�ؾ��� �����Ͱ� ������ SendPacket�Լ� ȣ��

    // echo�ϱ� ��Ŷ�� sendbuffer�� �ٷ� ����


    SendPacket(sessionID, pPacket);
}

void SendPacket(int playerID, CPacket* pPacket)
{
    // ���ڷ� ���� id�� ����� ���� �˻�
    EnterCriticalSection(&g_sessionMap_cs);
    auto iter = g_sessionMap.find(playerID);
    LeaveCriticalSection(&g_sessionMap_cs);

    if (iter == g_sessionMap.end())
        return;
    
    // �÷��̾� ID�� ���� ID�� �����ϱ⿡ ����Map���� ���� ID�� ���� ������ ã�� SendRingBuffer�� ��Ŷ�� �ִ� �����͸� ����.
    int retVal = iter->second->sendBuffer.Enqueue(pPacket->GetBufferPtr(), pPacket->GetDataSize());
    if (retVal == 0)
    {
        __debugbreak();
    }


    SendPost(iter->second);
}


void ReleaseSession(CSession* pSession)
{
    UINT32 sessionID = pSession->iSessionID;

    int iCnt;

    // ���� �ʿ��� ����
    EnterCriticalSection(&g_sessionMap_cs);
    EnterCriticalSection(&pSession->cs);
    LeaveCriticalSection(&pSession->cs);
    iCnt = g_sessionMap.erase(sessionID);
    LeaveCriticalSection(&g_sessionMap_cs);
    closesocket(pSession->sock);
    delete pSession;
    // ���� ����

    // ���� ��ü ����
}


// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(LPVOID arg)
{
    int retval;

    while (1) {
        // �񵿱� ����� �Ϸ� ��ٸ���
        DWORD cbTransferred{};
        //SOCKET client_sock;
        CSession* pSession = nullptr;

        // GQCS�� ��ȯ���� �����ϸ� TRUE, �ƴϸ� FALSE.
        OVERLAPPED* pOverlapped;
        memset(&pOverlapped, 0, sizeof(pOverlapped));

        // GQCS�� �Ϸ������� ������.
        retval = GetQueuedCompletionStatus(g_hIOCP, &cbTransferred,
            (PULONG_PTR)&pSession, &pOverlapped, INFINITE);


        EnterCriticalSection(&pSession->cs);
        

        int wsaError = WSAGetLastError();
        int Error = GetLastError();

        //// ���� ó��
        //DWORD dwError = WaitForSingleObject(g_hExitThreadEvent, 0);
        //if (dwError != WAIT_TIMEOUT)
        //    break;

        // ���� ó��
        if (retval == FALSE || cbTransferred == 0)
        {
            ;
        }

        // recv �Ϸ� ó��
        else if (pOverlapped == &pSession->overlappedRecv) {

            // recv ȣ��� wsaBuf�� recvBuffer�� �����͸� �־���⿡ �̹� �����Ͱ� ����Ǿ� �����Ƿ� rear�� cbTransferred ��ŭ �Űܼ� �����۰� �ùٸ��� �۵��ϵ��� ����
            pSession->recvBuffer.MoveRear(cbTransferred);

            // recv �����ۿ� �ִ� �����͸� ��Ŷ ������ ������
            while (ProcessMessage(pSession));
               
            // Recv�� �ɾ�ξ� �񵿱� ����� ����
            RecvPost(pSession);
        }

        // send �Ϸ� ó��
        else if (pOverlapped == &pSession->overlappedSend) {
            // sendBuffer���� ���� �Ϸ�� ��ŭ ����
            pSession->sendBuffer.MoveFront(cbTransferred);
            pSession->bSending = false;

            // �����۰� ��� ���� ������ send. 
            if (pSession->sendBuffer.GetUseSize() != 0)
                SendPost(pSession);
        }

        LeaveCriticalSection(&pSession->cs);

        // �Ϸ� ���� ó���� �����⿡ ī��Ʈ 1����
        if (InterlockedDecrement(&pSession->IOcount) == 0)
        {
            ReleaseSession(pSession);
        }
    }

    return 0;
}

unsigned int WINAPI AcceptThread(void* pArg)
{
    // ������ ��ſ� ����� ����
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;

    while (true)
    {
        // Accept
        int addrlen = sizeof(clientaddr);
        client_sock = accept(g_listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            std::cout << "accept()\n";
            break;
        }

        // ���� ����
        
        // ���� ���� ����ü �Ҵ�
        CSession* pSession = new CSession;

        if (pSession == NULL)
            break;
        
        // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
        CreateIoCompletionPort((HANDLE)client_sock, g_hIOCP, (ULONG_PTR)pSession, 0);



        // IP, PORT �� �߰�
        memcpy(pSession->IP, inet_ntoa(clientaddr.sin_addr), sizeof(pSession->IP));
        pSession->port = ntohs(clientaddr.sin_port);

        ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
        ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));

        ZeroMemory(pSession->wsaRecvBuf, sizeof(WSABUF) * 2);

        // �����۸� ����ϱ⿡ directEnq ���� �����ؼ� ���۸� �������Ѵ�.
        pSession->wsaRecvBuf[0].buf = pSession->recvBuffer.GetFrontBufferPtr();
        pSession->wsaRecvBuf[0].len = pSession->recvBuffer.DirectEnqueueSize();

        pSession->wsaRecvBuf[1].buf = pSession->recvBuffer.GetBufferPtr();
        pSession->wsaRecvBuf[1].len = pSession->recvBuffer.GetBufferCapacity() - pSession->recvBuffer.DirectEnqueueSize();

        pSession->sock = client_sock;
        pSession->IOcount = 0;


        // ���� �ʿ� ���� ��� �߰�, ���������� g_SessionID�� ������ �˻��� ���̱⿡ key������ �߰�
        EnterCriticalSection(&g_sessionMap_cs);
        g_sessionMap.emplace(g_SessionID, pSession);
        LeaveCriticalSection(&g_sessionMap_cs);
        pSession->iSessionID = g_SessionID;

        // �÷��̾� �߰�
        CPlayer* pPlayer = new CPlayer;
        g_playerMap.emplace(g_SessionID, pPlayer);
        pPlayer->m_playerID = g_SessionID;

        // ���� ID 1 ����
        g_SessionID++;


        // Recv�� �ɾ�ξ� �񵿱� ����� ����
        RecvPost(pSession);
    }

    return 0;
}

void RecvPost(CSession* pSession)
{
    int retval;
    DWORD flags{};

    // ó�� IO Count ++, �̴� �ڿ� �� ��� �߰��� ������ ��� ++�� ���ϰ� �ִٰ� IO Count�� 0�� �Ǵ� ��찡 �ֱ� ������ recv, send ȣ�� ���� �Ѵ�.
    InterlockedIncrement(&pSession->IOcount);

    //ZeroMemory(&pSession->wsaRecvBuf, sizeof(WSABUF) * 2);
    pSession->wsaRecvBuf[0].buf = pSession->recvBuffer.GetFrontBufferPtr();
    pSession->wsaRecvBuf[0].len = pSession->recvBuffer.DirectEnqueueSize();
    pSession->wsaRecvBuf[1].buf = pSession->recvBuffer.GetBufferPtr();
    pSession->wsaRecvBuf[1].len = pSession->recvBuffer.GetBufferCapacity() - pSession->recvBuffer.DirectEnqueueSize();

    // Recv�� �ɾ�ξ� �񵿱� ����� ����
    // �񵿱� I/O�� I/O�ϳ��� �ϳ��� ������ ����ü�� �ʿ��ϴ�.
    retval = WSARecv(pSession->sock, pSession->wsaRecvBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);
    if (retval == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
        InterlockedDecrement(&pSession->IOcount);
        //std::cout << "WSARecv() failed : " << WSAGetLastError() << "\n";
    }
}

void SendPost(CSession* pSession)
{
    // ó�� IO Count ++, �̴� �ڿ� �� ��� �߰��� ������ ��� ++�� ���ϰ� �ִٰ� IO Count�� 0�� �Ǵ� ��찡 �ֱ� ������ recv, send ȣ�� ���� �Ѵ�.
    InterlockedIncrement(&pSession->IOcount);

    // sendBuffer�� �ִ� �����͸� WSASend�� ����
    DWORD sendBytes = 0;
    DWORD flags = 0;

    if (pSession->bSending == true)
        return;

    DWORD cbTransferred = pSession->sendBuffer.GetUseSize();

    // ���� �����Ͱ� sendBuffer�� directDeq ������ ���� ũ�ٸ�
    if (cbTransferred > pSession->sendBuffer.DirectDequeueSize())
    {
        pSession->wsaSendBuf[0].buf = pSession->sendBuffer.GetFrontBufferPtr();
        pSession->wsaSendBuf[0].len = pSession->sendBuffer.DirectDequeueSize();

        pSession->wsaSendBuf[1].buf = pSession->sendBuffer.GetBufferPtr();
        pSession->wsaSendBuf[1].len = cbTransferred - pSession->sendBuffer.DirectDequeueSize();
    }
    // ���� �����Ͱ� sendBuffer�� directDeq ������� �۰ų� ���ٸ�
    else
    {
        pSession->wsaSendBuf[0].buf = pSession->sendBuffer.GetFrontBufferPtr();
        pSession->wsaSendBuf[0].len = cbTransferred;

        pSession->wsaSendBuf[1].buf = pSession->sendBuffer.GetBufferPtr();
        pSession->wsaSendBuf[1].len = 0;
    }

    // WSASend ȣ��
    pSession->bSending = true;

    int retval = WSASend(pSession->sock, pSession->wsaSendBuf, 2, nullptr, flags, &pSession->overlappedSend, NULL);
    if (retval == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING){
        InterlockedDecrement(&pSession->IOcount);
    }
}


