
#include <iostream>

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

#include <process.h>
#include <unordered_map>

#include "RingBuffer.h"


#define SERVERPORT 6000

// ���� ���� ������ ���� ����ü
class CSession
{
public:
    CSession(){
    }
    ~CSession(){
    }

public:
    SOCKET sock;

    USHORT port;
    char IP[17];

    CRingBuffer recvQ;
    CRingBuffer sendQ;

    OVERLAPPED overlappedRecv;
    OVERLAPPED overlappedSend;
};

class CPlayer
{
public:
    CPlayer() {}
    ~CPlayer() {}

public:
    UINT32 m_playerID;
};

SOCKET g_listenSocket; 
HANDLE g_hIOCP;

// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(void* arg);
unsigned int WINAPI AcceptThread(void* pArg);

int main(void)
{
    int retval;

    // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // CPU ���� Ȯ��
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    // int ProcessorNum = si.dwNumberOfProcessors;
    // ���μ��� ���� �̸����� IOCP Running �������� ������ �����ϸ鼭 �׽�Ʈ �غ� ��.
    
    // ����� �Ϸ� ��Ʈ ����
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 3);
    if (g_hIOCP == NULL)
    {
        std::cerr << "INVALID_HANDLE : g_hIOCP\n";
        return 1;
    }

    // socket()
    g_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listenSocket == INVALID_SOCKET)
    {
        std::cerr << "INVALID_SOCKET : listen_sock\n";
        return 1;
    }


    // recvbuf ũ�� ���� (�۽� ������ ũ�⸦ 0���� ������ �߰� �ܰ� ���۸� ���� �޸𸮿� ���� direct I/O�� ����)
    int recvBufSize = 0;
    if (setsockopt(g_listenSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << std::endl;
        closesocket(g_listenSocket);
        WSACleanup();
        return 1;
    }


    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = bind(g_listenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        std::cout << "bind()\n";

    // listen()
    retval = listen(g_listenSocket, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        std::cout << "listen()\n";






    HANDLE hAcceptThread;		// ���ӿ�û, ���⿡ ���� ó��	
    HANDLE hWorkerThread;		// ��Ŀ ������

    DWORD dwThreadID;

    hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, NULL, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);








    WCHAR ControlKey;

    //------------------------------------------------
    // ���� ��Ʈ��...
    //------------------------------------------------
    while (1)
    {
        ControlKey = _getwch();
        if (ControlKey == L'q' || ControlKey == L'Q')
        {
            //------------------------------------------------
            // ����ó��
            //------------------------------------------------
            PostQueuedCompletionStatus(g_hIOCP, 0, 0, NULL);
            break;
        }
    }






    // ���� ����
    WSACleanup();
    return 0;
}



// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(LPVOID arg)
{
    int retval;

    while (1) {
        // �񵿱� ����� �Ϸ� ��ٸ���
        DWORD transferredDataLen{};

        CSession* pSession = nullptr;

        // GQCS�� ��ȯ���� �����ϸ� TRUE, �ƴϸ� FALSE.
        OVERLAPPED* pOverlapped;
        memset(&pOverlapped, 0, sizeof(pOverlapped));

        // GQCS�� IOCP �ڵ�� �� �Ϸ������� ������.
        // ��ȯ������ �� ����Ʈ�� �޾Ҵ���, � Ű����, � ������ ����ü�� ����ߴ����� �˷��ش�.
        retval = GetQueuedCompletionStatus(g_hIOCP, &transferredDataLen, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

        int wsaError = WSAGetLastError();
        int Error = GetLastError();

        // ���� ó�� - GQCS�� �����߰ų�, cbTransferred�� 0�� ���(rst�� �� ���)
        if (retval == FALSE || transferredDataLen == 0 )
        {
            // PQCS�� ���� �Ϸ� ������(0, 0, NULL)�� �� ���. ��, ���� ���� ���� ���
            if (pSession == 0 && pOverlapped == nullptr)
            {
                // while���� ����������.
                break;
            }

            // ���濡�� ���� ��� ���ʿ��� Ȯ���غ��� ��Ʈ��ũ�� �� �̻� ���� ���
            if (wsaError == ERROR_NETNAME_DELETED)
            {
                continue;
            }

            // ��� ������ ���� ���� ��쳪 ��¥ ������ ���̴�. Ȯ�� �ʿ�.
            DebugBreak();
        }

        // recv �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedRecv) {
            // �����۸� ����ϱ⿡ ���ۿ� �����͸� �����ϰ�, ������� �������� �ʾҴ�. ��� ����� ������Ų��.
            pSession->recvQ.MoveRear(transferredDataLen);

            // ������������ ���� ���̱⿡ ���� �����͸� �̾Ƴ��� sendQ�� ����
            char temp[512];
            pSession->recvQ.Dequeue(temp, transferredDataLen);

            pSession->sendQ.Enqueue(temp, transferredDataLen);

            WSABUF wsaBuf[2];
            wsaBuf[0].buf = pSession->sendQ.GetFrontBufferPtr();
            wsaBuf[0].len = pSession->sendQ.DirectDequeueSize();

            wsaBuf[1].buf = pSession->sendQ.GetBufferPtr();

            // �������� �������� ���̰� sendQ���� �� �� �ִ� �������� ���̺��� ũ�ٸ�
            if (transferredDataLen > (DWORD)pSession->sendQ.DirectDequeueSize())
            {
                wsaBuf[1].len = transferredDataLen - wsaBuf[0].len;
            }
            // �������� �������� ���̰� sendQ���� �� �� �ִ� �������� ���̿� ������ �� �۰ų� ���ٸ�
            else
            {
                wsaBuf[1].len = 0;
            }

            DWORD flags{};
            WSASend(pSession->sock, wsaBuf, 2, NULL, flags, &pSession->overlappedSend, NULL);



            // �ٽ� recv�� �ɾ�ּ� �񵿱������� recv�� ó���ǵ��� ��
            wsaBuf[0].buf = pSession->recvQ.GetFrontBufferPtr();
            wsaBuf[0].len = pSession->recvQ.DirectEnqueueSize();
            wsaBuf[1].buf = pSession->recvQ.GetBufferPtr();
            wsaBuf[1].len = pSession->recvQ.GetBufferCapacity() - pSession->recvQ.DirectEnqueueSize();

            flags = 0;
            WSARecv(pSession->sock, wsaBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);
        }

        // send �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedSend) {
            // ������ ���� �Ϸ� �����Ƿ� sendQ�� �ִ� �����͸� ����
            pSession->sendQ.MoveFront(transferredDataLen);
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
        client_sock = accept(g_listenSocket, (SOCKADDR*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            std::cout << "accept()\n";
            break;
        }

        // ���� ����
        
        // ���� ���� ����ü �Ҵ�
        CSession* pSession = new CSession;

        if (pSession == NULL)
            break;

        // IP, PORT �� �߰�
        char pAddrBuf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &clientaddr, pAddrBuf, sizeof(pAddrBuf)) == NULL) {
            std::cout << "Error : inet_ntop()\n";
            DebugBreak();
        }

        memcpy(pSession->IP, pAddrBuf, sizeof(char) * 16);
        pSession->IP[16] = '\0';

        pSession->port = ntohs(clientaddr.sin_port);

        // send/recv�� ������ ����ü �ʱ�ȭ
        ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
        ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));

        // client ���� ����
        pSession->sock = client_sock;


        // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
        CreateIoCompletionPort((HANDLE)client_sock, g_hIOCP, (ULONG_PTR)pSession, 0);

        DWORD flags{};

        WSABUF recvBuf;
        recvBuf.buf = pSession->recvQ.GetFrontBufferPtr();
        recvBuf.len = pSession->recvQ.GetBufferCapacity();

        // �񵿱� recv ó���� ���� ���� recv�� �ɾ��. ���� �����ʹ� wsaBuf�� ����� �޸𸮿� ä���� ����.
        WSARecv(pSession->sock, &recvBuf, 1, NULL, &flags, &pSession->overlappedRecv, NULL);
    }

    return 0;
}
