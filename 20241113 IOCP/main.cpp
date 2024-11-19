
#include "pch.h"

#include <process.h>
#include <unordered_map>

#include "Session.h"
#include "RingBuffer.h"

#include "ServerLib.h"
#include "Content.h"

#define SERVERPORT 6000


SOCKET g_listenSocket; 
HANDLE g_hIOCP; 
UINT32 g_id = 0;

// key�� id�� �޾� �����Ǵ� ���� ��
std::unordered_map<UINT32, CSession*> g_clients;

// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(void* pArg);
unsigned int WINAPI AcceptThread(void* pArg);

ServerContent* g_pContent = nullptr;
ServerLib* g_pServer = nullptr;




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
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
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



    
    ServerLib serverLib;        // IOCP ���� ���̺귯�� �ν��Ͻ�
    ServerContent serverContent; // ���� ������ �ν��Ͻ�
    serverLib.RegisterIContent(&serverContent);
    serverContent.RegisterIServer(&serverLib);

    g_pContent = &serverContent;
    g_pServer = &serverLib;


    HANDLE hAcceptThread;		// ���ӿ�û, ���⿡ ���� ó��	
    HANDLE hWorkerThread;		// ��Ŀ ������

    DWORD dwThreadID;

    hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, &serverLib, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
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

        if (ControlKey == L'w' || ControlKey == L'W')
        {
            //------------------------------------------------
            // �����ϳ� �����ؼ� WSASend�� send 0�� ���� �� �Ϸ������� ������ Ȯ��
            //------------------------------------------------
            auto iter = g_clients.find(0);
            WSABUF wsaBuf;
            wsaBuf.buf = iter->second->sendQ.GetFrontBufferPtr();
            wsaBuf.len = 0;

            DWORD flags{};
            WSASend(iter->second->sock, &wsaBuf, 1, NULL, flags, &iter->second->overlappedSend, NULL);
            break;
        }
    }






    // ���� ����
    WSACleanup();
    return 0;
}

UINT32 ReleaseSession(CSession* pSession)
{
    if (pSession->sock != INVALID_SOCKET)
    {
        closesocket(pSession->sock);
        pSession->sock = INVALID_SOCKET;
    }

    return InterlockedDecrement(&pSession->IOCount);
}


// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(void* pArg)
{
    int retval;
    int error;
    UINT32 IOCount;

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

        EnterCriticalSection(&pSession->cs);


        // PQCS�� ���� �Ϸ� ������(0, 0, NULL)�� �� ���. ��, ��(main ������)�� ���� ���� ���
        // �Ϸ������� �ִ� ���� �����带 ����.
        if ((transferredDataLen == 0 && pSession == 0 && pOverlapped == nullptr))
            break;

        // ���� ó�� - GQCS�� ������ ���, send,recv �ϴ��� ������ ���ܼ� closesocket�� �ؼ� �ش� ������ ����ϴ� �Ϸ������� ��� ������ ����̴�.
        // transferredDataLen�� 0�� ���(rst�� �� ���) <- �̰� send �ʿ��� 0�� ���������� ���� �� ������ send���� 0�� ������ ���� ������ ó��
        if (retval == FALSE || transferredDataLen == 0)
        {
            // ������ ��ȿ�ϴٸ� closesocket �ϰ�, ������ IO Count�� 1 �����Ѵ�. 
            IOCount = ReleaseSession(pSession);

            // IO Count�� 0�̶�� ������ ����.
            if (IOCount == 0)
            {
                // ���� ����
                g_pServer->DeleteSession(pSession);
            }

            // IO Count�� 0�� �ƴ϶�� �Ϸ������� ���� ������ GQCS���� �Ϸ������� �ٽ� ���������� continue�Ѵ�.
            LeaveCriticalSection(&pSession->cs);
            continue;
        }

        // recv �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedRecv) {

            // WSARecv�� �����۸� ����ϱ⿡ ���ۿ� �����͸� �����ϰ�, ������� �������� �ʾҴ�. ��� ����� ������Ų��.
            pSession->recvQ.MoveRear(transferredDataLen);

            while (true)
            {
                //// ���߿� �� �κ� �޽����� ���̿� ���Ͽ� ������������ ����
                //if (pSession->recvQ.GetUseSize() < transferredDataLen)
                //    break;
                if (pSession->recvQ.GetUseSize() == 0)
                    break;

                // recvQ�� ���� �����͸� �̾Ƴ��� Packet�� ����
                CPacket Packet; // �� �Ŵ����� ���� ������ ��� ���. ����� �غ��� ������������ ���� ������ ��� ����ϰ� ����.
                int recvQDeqRetVal = pSession->recvQ.Dequeue(Packet.GetBufferPtr(), transferredDataLen);
                Packet.MoveWritePos(transferredDataLen);

                if (recvQDeqRetVal != transferredDataLen)
                {
                    DebugBreak();
                }

                // ������ �ڵ� OnRecv�� ������ id�� ��Ŷ ������ �Ѱ���
                g_pContent->OnRecv(pSession->id, &Packet);

                // send ó��
                g_pServer->SendPost(pSession);
            }

            // recv ó��
            g_pServer->RecvPost(pSession);
        }

        // send �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedSend) {
            // sendFlag�� ���� ����, sendQ�� ���� �����Ͱ� �ִ��� Ȯ���Ѵ�. 
            // �ٸ� �����忡�� recv �Ϸ������� ó���� �� sendQ�� enq �ϰ�, sendFlag�� �˻��ϱ⿡
            // �� �� �ϳ��� ������ sendFlag�� �ùٸ��� ����ȴ�.
            InterlockedExchange(&pSession->sendFlag, 0);

            // ������ ���� �Ϸ� �����Ƿ� sendQ�� �ִ� �����͸� ����
            pSession->sendQ.MoveFront(transferredDataLen);

            // ���� sendQ�� �����Ͱ� �ִٸ�
            if (pSession->sendQ.GetUseSize() != 0)
            {
                if (InterlockedExchange(&pSession->sendFlag, 1) == 0)
                {
                    // send ó��
                    g_pServer->SendPost(pSession);
                }
            }

            // ���� sendQ�� �����Ͱ� ���ٸ�
            else
                ; // �̹� sendFlag 0���� �ٲ����� �ƹ��͵� �� ���� ����.
        }

        LeaveCriticalSection(&pSession->cs);

        // �Ϸ����� ó�� ���� IO Count 1 ����
        IOCount = InterlockedDecrement(&pSession->IOCount);

        // IO Count�� 0�� ���, ��� �Ϸ������� ó�������Ƿ� ������ �����ص� ����.
        if (IOCount == 0)
        {
            // ���� ����
            g_pServer->DeleteSession(pSession);
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

        // IO Count �ο�, �ʱ�ȭ �� �ñ�� ���� ��ϵ� ���̴� 0���� ����
        pSession->IOCount = 0;

        // send�� 1ȸ �����ϱ� ���� flag ����
        pSession->sendFlag = 0;

        pSession->SendingCount = 0;


        // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
        CreateIoCompletionPort((HANDLE)client_sock, g_hIOCP, (ULONG_PTR)pSession, 0);


        // ���⼭ �������� ���� ���̳� �迭�� ������ ���
        g_pServer->RegisterSession(pSession);

        // recv ó��, �� ����� �����ϰ�, ���Ŀ� WSARecv�� �ϳĸ�, ��� ���� Recv �Ϸ������� �� �� ����. �׷��� ����� ������
        g_pServer->RecvPost(pSession);

        // �� �������� IO Count�� �̱� �����峪 �ٸ� ���⿡ interlock���� ������ �˻� ����.
        if (pSession->IOCount == 0)
        {
            // ���� WSARecv�� �����ߴٸ� �Ϸ������� ���� �ʱ⿡ ���⼭ �ٷ� ������ ����������Ѵ�.
            g_pServer->DeleteSession(pSession);
            continue;
        }
    }

    return 0;
}


