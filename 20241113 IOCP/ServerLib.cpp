
#include "pch.h"
#include "ServerLib.h"

#include <process.h>

#include "Session.h"
#include "Protocol.h"
#include "Packet.h"

#include "Managers.h"
#include "ContentManager.h"

bool CGameServer::Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, int maxSessionCount, int pendingAcceptCount)
{
    // Ÿ�̸� ���͹��� 1ms�� ����. ������ ������ ��⿡ Ÿ�̸� ���͹��� �ٿ� �񵿱� �۾��� ������ ������ �� Ȯ�εǱ��� �ɸ��� �ð��� �������μ� �����ϴ� �ð��� ������ ������ �ö󰣴ٰ� ����.
    // �ƹ�ư ������ �� ���� ���� �� �������� ������ 30%�̻� �ö󰡴� ���� Ȯ��. ���� ������ �������...
    timeBeginPeriod(1);

    //=================================================================================================================
    // 1. ���� ���� �ʱ�ȭ �� IOCP ����
    //=================================================================================================================

    // winSock �ʱ�ȭ
    InitWinSock();

    // listenSocket ����
    CreateListenSocket(PROTOCOL_TYPE::TCP_IP);

    // IOCP handle ����
    CreateIOCP(runningThreadCount);

    //=================================================================================================================
    // 2. ���� ���� �ɼ� ���� �� bind / listen 
    //=================================================================================================================
    
    // ���� �ɼ� ����
    SetOptions(nagleOption);

    // listen ���Ͽ� ������ ������ NIC, PORT ��ȣ bind
    Bind(ip, port);

    // listen ���� ����
    Listen();

    //=================================================================================================================
    // 3. ���� ������ �ʿ��� ������ �ε�
    //=================================================================================================================

    InitResource(maxSessionCount);

    //=================================================================================================================
    // 4. ������ Ǯ ����
    //=================================================================================================================

    CreateThreadPool(workerThreadCount);

    //=================================================================================================================
    // 5. pendingAcceptCount ������ŭ ���� ���� �� acceptex ȣ��
    //=================================================================================================================

    ReserveAcceptSocket(pendingAcceptCount);

    return true;  // ���� �� true ��ȯ
}

void CGameServer::Stop(void)
{
    // ��� ������ ���� �� ���ҽ� ����

    // �� �Լ��� ȣ���� �����尡 ���� accept�� ����, ������ ���ǵ��� ������ ����.
    // ������ ������ �� ���������� Ȯ���ϰ�, PQCS�� ��Ŀ ������ ������ŭ ȣ��, ���� WFMOS�� ȣ���ϰ� ��ٸ�

    // ��� ��Ŀ �����尡 �������ٸ� ���ҽ��� �����ϰ� ������ ������ �ִ��� Ȯ�� �� ������ ����.
    for (int i = 0; i < threadCnt; ++i)
        PostQueuedCompletionStatus(hIOCP, 0, 0, NULL);

    // ===========================================================================
    // 1. ��� �����尡 ����Ǳ⸦ ���
    // ===========================================================================
    DWORD retVal = WaitForMultipleObjects(threadCnt + 1, hThreads, TRUE, INFINITE);     // accept ������ ���� 1 �߰��ؼ� �˻�
    DWORD retError = GetLastError();

    // ===========================================================================
    // 2. ���ҽ� ����
    // ===========================================================================
    ReleaseResource();

    // ===========================================================================
    // 3. �����尡 ���� ����Ǿ����� �˻�
    // ===========================================================================
    DWORD ExitCode;

    // �� �Ʒ��� �ڵ�� �߸� �ȵ�. ��ٴ� ���� main�� ����Ǿ������� �����尡 ����ִٴ� �ǹ�.
    wprintf(L"\n\n--- THREAD CHECK LOG -----------------------------\n\n");

    GetExitCodeThread(hThreads[0], &ExitCode);
    if (ExitCode != 0)
        wprintf(L"error - Accept Thread not exit\n");

    for (UINT8 i = 1; i < threadCnt; ++i)
    {
        GetExitCodeThread(hThreads[i], &ExitCode);
        if (ExitCode != 0)
            wprintf(L"error - IO Thread not exit\n");
    }

    // ���� ����
    WSACleanup();
}

void CGameServer::ReleaseResource(void)
{
    // ���� ����
    delete[] sessions;

    DeleteCriticalSection(&cs_sessionID);
}

bool CGameServer::Disconnect(UINT64 sessionID)
{
    // ���� ���� ó��, ������ �ʿ��� ���� ȣ���ϴ� �Լ��� ������ ��ü�� �Ҹ�ǰ� ���� ������ �˷��ִ� ����.
    // �������� �Ϸ������� ��� �;� �����ǹǷ� �켱 ���� ������ flag�� �÷��ΰ� IO Count�� 0�� �Ǹ� closesocket�� ������ �Ѵ�.
    
    

    return true;
}

void CGameServer::SendPacket(UINT64 sessionID, CPacket* pPacket)
{
    DWORD curThreadID = GetCurrentThreadId();

    // ���� �˻�
    UINT16 index = static_cast<UINT16>(sessionID);
    CSession* pSession = &sessions[index];

    // ������ ã�Ҵٸ�
    if (pSession->id == sessionID)
    {
        // ��Ŷ �Ҵ�
        CPacket* pSendPacket = new CPacket;

        // ��� ���� ����
        PACKET_HEADER header;
        header.bySize = pPacket->GetDataSize();
        pSendPacket->PutData((char*)&header, sizeof(PACKET_HEADER));

        // ���ڷ� ���� ��Ŷ�� ������ ����
        pSendPacket->PutData(pPacket->GetFrontBufferPtr(), pPacket->GetDataSize());

        // sendQ�� ����ȭ ���� ��ü�� ����
        pSession->sendQ.Enqueue(pSendPacket);
    }
    // �ƴ϶�� ��Ȱ��Ǹ鼭 �̻��� ���� ���� �� �ִ�.
    else
    {
        DebugBreak();
    }

    SendPost(pSession);
}

// ������ ��������� ���� ����� �޴� �Լ�. ���� �׽�Ʈ�� ���� ���� �ʿ��� �� �系 IP�� ������ �� �ֵ��� �ο��� �����ϴ� ���.
bool CGameServer::OnConnectionRequest(const std::string& ip, int port)
{
    std::string str{ "1.0.8.0" };
    if (ip == str)
        return false;


    // ������ �׽�Ʈ�� ���� �ϴ� true�� ��ȯ
    return true;

    // ���ڷ� �����ϴ� Ŭ���̾�Ʈ�� IP�� PORT�� �ѱ��, �� �Լ� �ȿ��� Ư�� IP�� ���ӿ� ���� ���� ó���Ѵ�.

    // ó�� ������ �������� ���
    if (isServerMaintrenanceMode)
    {
        // ������ IP�� allowedIPs�� ��ϵ� IP�� ��� ������ ���, �ƴ� ��� ����
        const auto& iter = allowedIPs.find(ip);

        if (iter != allowedIPs.end())
            return true;
    }
    // ���� ������ ���� ���
    else
    {
        // ������ IP�� blockedIPs�� ��ϵ� IP�� �ƴ� ��� ������ ���, �ƴ� ��� ����
        const auto& iter = blockedIPs.find(ip);

        if (iter == blockedIPs.end())
            return true;
    }

    return false;
}

void CGameServer::OnAccept(UINT64 sessionID)
{
    // ���� �׽�Ʈ�� ���� ó�� ������ �� ������ data�� Ŭ�� ����.
    CPacket packet;
    UINT64 data = 0x7fffffffffffffff;
    packet.PutData((char*)&data, sizeof(data));

    // ��Ŷ�� ������ �Լ� 
    SendPacket(sessionID, &packet);
}

void CGameServer::OnRelease(UINT64 sessionID)
{
    // �ش� ���� id�� ���� �÷��̾ �˻�

    // �ش� �÷��̾ ����

    // ���⼱ ���� �׽�Ʈ �����ϱ� �ƹ��͵� ���� ����.
}

void CGameServer::OnRecv(UINT64 sessionID, CPacket* pPacket)
{
    // ���⼭ ������ ó���� ��.
    // ~ ������ ~
    // ������ ó���� �ϸ鼭 ��Ŷ�� ����� SendPacket�� ȣ��
    // sendQ�� �����͸� ����. + sending ���θ� �Ǵ��� sendQ�� ���� �����͸� ���� �� �ִٸ� ������.

    SendPacket(sessionID, pPacket);
}

void CGameServer::InitWinSock(void) noexcept
{
    // ������ ���� �ʱ�ȭ
    WSADATA wsaData;

    int retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (retVal != 0) {
        std::cout << "Error : WSAStartup failed " << retVal << ", " << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::CreateListenSocket(PROTOCOL_TYPE type)
{
    switch (type)
    {
    case PROTOCOL_TYPE::TCP_IP:
        // �̹� 2���ڿ��� TCP�� UDP�� ������ ���� ������ 3���ڿ� ���� �ʾƵ� �����ϴ�.
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET)
        {
            std::cerr << "Error : TCP CreateListenSocket()" << WSAGetLastError() << "\n";
            DebugBreak();
        }
        break;
    case PROTOCOL_TYPE::UDP:
        listenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (listenSocket == INVALID_SOCKET)
        {
            std::cerr << "Error : UDP CreateListenSocket()" << WSAGetLastError() << "\n";
            DebugBreak();
        }
        break;
    default:
        std::cerr << "Error : default socket()" << WSAGetLastError() << "\n";
        DebugBreak();
        break;
    }
}

void CGameServer::CreateIOCP(int runningThreadCount)
{
    // CPU ���� Ȯ��
    //SYSTEM_INFO si;
    //GetSystemInfo(&si);
    // int ProcessorNum = si.dwNumberOfProcessors;
    // ���μ��� ���� �̸����� IOCP Running �������� ������ �����ϸ鼭 �׽�Ʈ �غ� ��.

    // ����� �Ϸ� ��Ʈ ����
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, runningThreadCount);
    if (hIOCP == NULL)
    {
        std::cerr << "INVALID_HANDLE : hIOCP, Error : " << WSAGetLastError() << "\n";
        DebugBreak();
    }

    Managers::GetInstance().Content()->InitWithIOCP(hIOCP);
}

void CGameServer::Bind(unsigned long ip, UINT16 port)
{
    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = ip; //inet_addr(ip);   htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    int retval = bind(listenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));

    if (retval == SOCKET_ERROR)
    {
        std::cout << "Error : Bind(), " << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::Listen(void)
{
    // listen()
    int retval = listen(listenSocket, SOMAXCONN_HINT(65535));
    if (retval == SOCKET_ERROR)
    {
        std::cout << "Error : Listen(), " << WSAGetLastError() << "\n";
        DebugBreak();
    }

    // ���� ������ IOCP�� ���. AcceptEx�� ����ϱ� ���ؼ� ���� ������ IOCP�� ����������
    HANDLE hResult = CreateIoCompletionPort(
        (HANDLE)listenSocket,  // listen() �� ���� �ڵ�
        hIOCP,                 // ���� hIOCP �ڵ鿡 ����
        (ULONG_PTR)0,          // CompletionKey (0 Ȥ�� �ĺ��� ��)
        0                      // ������ Ǯ ũ�� ���� (0 ���� �θ� �⺻��)
    );

    // ���ϰ� �˻�
    if (hResult == NULL) {
        DWORD err = GetLastError();
        std::cout << "[����] CreateIoCompletionPort ����: " << err << "\n";
        // �ʿ� �� ���α׷� ���� �Ǵ� ���� ����

        DebugBreak();
    }
}

void CGameServer::InitResource(int maxSessionCount)
{
    // ���⼭ ���ǿ� ���õ� ��� �������� �ʱ�ȭ. ���� Ǯ�� �ʿ��ϴٸ� ���� �ڵ� ����
    maxConnections = maxSessionCount;
    sessions = new CSession[maxConnections];

    // stack ������� ���� ID�� �迭 ������ ����( 0 ~ maxConnections ������ ���� �ε����� ��� )
    for (int i = maxConnections - 1; i >= 0; --i)
    {
        stSessionIndex.push(i);
    }

    isServerMaintrenanceMode = true;

    InitializeCriticalSection(&cs_sessionID);
}

void CGameServer::CreateThreadPool(int workerThreadCount)
{
    threadCnt = workerThreadCount;
    hThreads = new HANDLE[threadCnt + 1];

    DWORD dwThreadID;

    // Accept Thread ȣ��
    hThreads[0] = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, (unsigned int*)&dwThreadID);

    // Worker Threads ȣ��
    for (UINT8 i = 0; i < workerThreadCount; ++i)
    {
        hThreads[i + 1] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, (unsigned int*)&dwThreadID);
    }
}

void CGameServer::ReserveAcceptSocket(int pendingAcceptCount)
{
    maxPendingSessionCnt = pendingAcceptCount;

    // �ʱ� ���� ��� ���� ����ŭ PostAccept
    for (int i = 0; i < pendingAcceptCount; ++i)
    {
        CSession* pSession = FetchSession();
        InitSessionInfo(pSession);   // �ʿ��� �ʱ�ȭ��

        // �߰��� AcceptEx ���н�
        if (!AcceptPost(pSession))
        {
            // ���� �� �ٽ� �õ�
            returnSession(pSession);
            --i;

            continue;
        }

        // AcceptEx ȣ�⿡ ���������Ƿ� IOCount�� 1 ����. Accept �Ϸ����� ó�� ���� ������ ����
        //InterlockedIncrement(&pSession->IOCount);

        // �� �������� curPendingSessionCnt�� ��Ŀ�����忡�� ������ �Ͼ �� �ֱ⿡ interlocked�� ���
        // �ش� ���� data race ����� �����Ƿ� ineterlock ���
        InterlockedIncrement(&curPendingSessionCnt);
    }
}

void CGameServer::SetTCPSendBufSize(SOCKET socket, UINT32 size)
{
    int recvBufSize = size;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR)
    {
        std::cerr << "setsockopt failed with error : " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        WSACleanup();
        DebugBreak();
    }
}

void CGameServer::SetNonBlockingMode(SOCKET socket, bool bNonBlocking)
{
    u_long mode = bNonBlocking; // 1: Non-blocking, 0: Blocking
    if (ioctlsocket(listenSocket, FIONBIO, &mode) != 0) {
        std::cerr << "Failed to set non-blocking mode. Error : " << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::DisableNagleAlgorithm(SOCKET socket, bool bNagleOn)
{
    int flag = !bNagleOn; // 1: Nagle ��Ȱ��ȭ, 0: �⺻�� 0���� Nagle�� ���� �ִ� ��Ȳ
    // Nagle �ɼ��� true��� Nagle�� Ű�� ��Ȳ�̰�, flag == 0, flag ���� 0�̿��� Ŵ
    // Nagle �ɼ��� false��� Nagle�� ���� ��Ȳ�̴�. flag == 1, flag ���� 1�̸� ��
    if (setsockopt(listenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag)) != 0) {
        std::cerr << "Failed to disable Nagle algorithm. Error : " << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::EnableRSTOnClose(SOCKET socket)
{
    // �⺻���� l_onoff == 0 �̶� ���� ���ص� RST�� ���ư���. ��������� ȣ���� �غ��� �;���.
    struct linger lingerOpt = { 0, 0 }; // l_onoff = 1, l_linger = 0
    if (setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(lingerOpt)) != 0) {
        std::cerr << "Failed to enable RST on close. Error :" << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::SetOptions(bool bNagleOn)
{
    // sendbuf ũ�� ���� (�۽� ������ ũ�⸦ 0���� ������ �߰� �ܰ� ���۸� ���� �޸𸮿� ���� direct I/O�� ����)
    SetTCPSendBufSize(listenSocket, 0);

    // blocking, non-blocking ���� ��� ��ȯ
    // accept ������� ���� ���� ���� �Ͼ�°� ȿ�����̱⿡ listen ������ blokcing �������� ����.
    SetNonBlockingMode(listenSocket, false);

    // RST Ȱ��, �⺻������ Ȱ���Ǿ� �ֱ⿡ �׳� ȣ���غ��� ��
    EnableRSTOnClose(listenSocket);

    // nagle �ɼ� Ȱ��/��Ȱ��. bNagleOn�� true�� �����°�. �⺻�� ���� �ֱ⿡ ��� false���� ���ǹ��� ��ȭ�� ����.
    DisableNagleAlgorithm(listenSocket, bNagleOn);
}

CSession* CGameServer::FetchSession(void)
{
    UINT64 sessionID = 0;

    // ���� 6byte�� ���Ӹ��� 1�� �����ϴ� uniqueSessionID ���� �ִ´�.

    // ��ü 64bit�� ���� 48bit�� ���
    sessionID = uniqueSessionID;
    sessionID <<= 16;

    // ����� uniqueSessionID�� �ߺ� ������ ���� 1����
    uniqueSessionID++;

    EnterCriticalSection(&cs_sessionID);

    // ������� �ʴ� �迭 �ε����� ã��
    UINT16 sessionIndex;
    while (!stSessionIndex.pop(sessionIndex));

    LeaveCriticalSection(&cs_sessionID);

    // �������� �� 1 ����
    InterlockedIncrement(&curSessionCnt);

    sessionID |= sessionIndex;
    sessions[sessionIndex].id = sessionID;

    return &sessions[sessionIndex];
}

void CGameServer::returnSession(CSession* pSession)
{
    EnterCriticalSection(&cs_sessionID);

    // sendQ�� ���� ó������ ���� ��Ŷ�� ����
    pSession->sendQ.ClearQueue();

    // ���� close
    closesocket(pSession->sock);
    pSession->sock = INVALID_SOCKET;

    // ���ڷ� ���� ������ ��ġ�� �迭 �ε����� ��ȯ
    stSessionIndex.push(static_cast<UINT16>(pSession->id));

    LeaveCriticalSection(&cs_sessionID);

    // �������� �� 1 ����
    InterlockedDecrement(&curSessionCnt);

    // ������ ������ �� 1 ����
    InterlockedIncrement(&disconnectedSessionCnt);

    // ������ ���������� ���Ŀ� ���������� ������ �����Ǿ����� �˸��� �ڵ尡 ��
    OnRelease(pSession->id);

    // ������ �����Ǿ����Ƿ� curPendingSessionCnt 1 ����
    InterlockedDecrement(&curPendingSessionCnt);
}

void CGameServer::InitSessionInfo(CSession* pSession)
{
    // send/recv�� ������ ����ü �ʱ�ȭ
    ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
    ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));
    ZeroMemory(&pSession->overlappedAccept, sizeof(pSession->overlappedAccept));

    ZeroMemory(&pSession->acceptBuffer, sizeof(pSession->acceptBuffer));

    // ������ ���� IO Count �ο�, �ʱ�ȭ �� �ñ�� ���� ��ϵ� ���̴� 0���� ����
    UINT32 IOCount = InterlockedExchange(&pSession->IOCount, 0);
    Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

    if (IOCount != 0)
        DebugBreak();

    // send�� 1ȸ �����ϱ� ���� flag ����
    pSession->sendFlag = 0;

    // ������ �ʱ�ȭ
    pSession->sendQ.ClearQueue();
    pSession->recvQ.ClearBuffer(); 
    
    // �ش� ������ ����ִ��� ����, ù ���� TRUE�� ������ �ʿ��� ������ ��쿡�� 0���� �����Ѵ�.
    pSession->isAlive = 1;
}

void CGameServer::loadIPList(const std::string& filePath, std::unordered_set<std::string>& IPList)
{
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        if (!line.empty()) {
            IPList.emplace(line);
        }
    }

    inputFile.close();

    return;
}
