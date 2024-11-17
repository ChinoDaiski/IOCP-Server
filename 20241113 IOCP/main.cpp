
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

    UINT32 id;

    UINT32 IOCount;
};

SOCKET g_listenSocket; 
HANDLE g_hIOCP; 
UINT32 g_id = 0;

// key�� id�� �޾� �����Ǵ� ���� ��
std::unordered_map<UINT32, CSession*> g_clients;

// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(void* pArg);
unsigned int WINAPI AcceptThread(void* pArg);

void SendPost(CSession* pSession);
void RecvPost(CSession* pSession);

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


    // sendbuf ũ�� ���� (�۽� ������ ũ�⸦ 0���� ������ �߰� �ܰ� ���۸� ���� �޸𸮿� ���� direct I/O�� ����)
    int recvBufSize = 0;
    if (setsockopt(g_listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR) {
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
unsigned int WINAPI WorkerThread(void* pArg)
{
    int retval;
    int error;

    // �񵿱� ����� �Ϸ� ��ٸ���
    DWORD transferredDataLen;

    CSession* pSession;

    // GQCS�� ��ȯ���� �����ϸ� TRUE, �ƴϸ� FALSE.
    OVERLAPPED* pOverlapped;

    while (1) {
        transferredDataLen = 0;
        memset(&pOverlapped, 0, sizeof(pOverlapped));

        // GQCS�� IOCP �ڵ�� �� �Ϸ������� ������.
        // ��ȯ������ �� ����Ʈ�� �޾Ҵ���, � Ű����, � ������ ����ü�� ����ߴ����� �˷��ش�.
        retval = GetQueuedCompletionStatus(g_hIOCP, &transferredDataLen, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

        error = WSAGetLastError();

        // ���� ó�� - GQCS�� �����߰ų�, cbTransferred�� 0�� ���(rst�� �� ���)
        if (retval == FALSE || transferredDataLen == 0 )
        {
            // PQCS�� ���� �Ϸ� ������(0, 0, NULL)�� �� ���. ��, ���� ���� ���� ���
            if (pSession == 0 && pOverlapped == nullptr)
            {
                // while���� ����������. �̷��� �Ϸ������� ���� �־ ������� ����ȴ�.
               
                // �Ϸ����� ó�� ���� IO Count 1 ����
                pSession->IOCount--;

                // IO Count�� 0�� ��� 
                if (pSession->IOCount == 0)
                {
                    // ���� �ݱ�
                    closesocket(pSession->sock);

                    // ���� ����
                    delete pSession;
                }
                else
                {
                    // �� �κ��� Ȯ���� �����̴�. �����Ǿ���ϴ� ��Ȳ���� IOCount�� 0�� �ƴϴϱ�. 
                    // �Ϸ������� �� ó�� ���ߴµ� ���� ������ �� ���, �̷� �� �ִ�. 
                    // �׷��� �� �κ��� ���� ���ؼ� debugbreak�� ����.
                    DebugBreak();
                }
                break;
            }

            // ���濡�� ���� ��� ���ʿ��� Ȯ���غ��� ��Ʈ��ũ�� �� �̻� ���� ���
            if (error == ERROR_NETNAME_DELETED)
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

            // send ó��
            SendPost(pSession);

            // recv ó��
            RecvPost(pSession);
        }

        // send �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedSend) {
            // ������ ���� �Ϸ� �����Ƿ� sendQ�� �ִ� �����͸� ����
            pSession->sendQ.MoveFront(transferredDataLen);
        }

        // �Ϸ����� ó�� ���� IO Count 1 ����
        pSession->IOCount--;

        // IO Count�� 0�� ��� 
        if (pSession->IOCount == 0)
        {
            // ���� �ݱ�
            closesocket(pSession->sock);

            // ���� ����
            delete pSession;
        }
    }

    return 0;
}

unsigned int WINAPI AcceptThread(void* pArg)
{
    // ������ ��ſ� ����� ����
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;

    int addrlen;

    while (true)
    {
        // Accept
        addrlen = sizeof(clientaddr);
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

        // client ID �ο�
        pSession->id = g_id;
        g_id++;

        // IO Count �ξ�
        pSession->IOCount = 0;



        // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
        CreateIoCompletionPort((HANDLE)client_sock, g_hIOCP, (ULONG_PTR)pSession, 0);


        // recv ó��
        RecvPost(pSession);
    }

    return 0;
}


void SendPost(CSession* pSession)
{
    int retval;
    int error;

    WSABUF wsaBuf[2];
    wsaBuf[0].buf = pSession->sendQ.GetFrontBufferPtr();
    wsaBuf[0].len = pSession->sendQ.DirectDequeueSize();

    wsaBuf[1].buf = pSession->sendQ.GetBufferPtr();

    int transferredDataLen = pSession->sendQ.GetUseSize();

    // �������� �������� ���̰� sendQ���� �� �� �ִ� �������� ���̺��� ũ�ٸ�
    if (transferredDataLen > pSession->sendQ.DirectDequeueSize())
    {
        wsaBuf[1].len = transferredDataLen - wsaBuf[0].len;
    }
    // �������� �������� ���̰� sendQ���� �� �� �ִ� �������� ���̿� ������ �� �۰ų� ���ٸ�
    else
    {
        wsaBuf[1].len = 0;
    }

    DWORD flags{};
    pSession->IOCount++;
    retval = WSASend(pSession->sock, wsaBuf, 2, NULL, flags, &pSession->overlappedSend, NULL);

    error = WSAGetLastError();
    if (retval == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            ;
        else
            DebugBreak();
    }
}

void RecvPost(CSession* pSession)
{
    int retVal;
    int error;

    DWORD flags{};

    WSABUF recvBuf[2];
    recvBuf[0].buf = pSession->recvQ.GetFrontBufferPtr();
    recvBuf[0].len = pSession->recvQ.DirectEnqueueSize();
    recvBuf[1].buf = pSession->recvQ.GetBufferPtr();
    recvBuf[1].len = pSession->recvQ.GetBufferCapacity() - pSession->recvQ.DirectEnqueueSize();
   
    pSession->IOCount++;
    // �񵿱� recv ó���� ���� ���� recv�� �ɾ��. ���� �����ʹ� wsaBuf�� ����� �޸𸮿� ä���� ����.
    retVal = WSARecv(pSession->sock, recvBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        DebugBreak();
    }
}

