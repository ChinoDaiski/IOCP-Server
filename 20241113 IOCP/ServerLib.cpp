
#include "pch.h"
#include "ServerLib.h"

#include <process.h>

#include "Session.h"
#include "Protocol.h"

bool CGameServer::Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, UINT16 maxSessionCount)
{
    //=================================================================================================================
    // 1. ���� ���� �ʱ�ȭ �� IOCP ����
    //=================================================================================================================

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
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, runningThreadCount);
    if (hIOCP == NULL)
    {
        std::cerr << "INVALID_HANDLE : hIOCP\n";
        return false;
    }

    // socket()
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cerr << "INVALID_SOCKET : listen_sock\n";
        return false;
    }


    // sendbuf ũ�� ���� (�۽� ������ ũ�⸦ 0���� ������ �߰� �ܰ� ���۸� ���� �޸𸮿� ���� direct I/O�� ����)
    int recvBufSize = 0;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }


    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = ip; //inet_addr(ip);   htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    retval = bind(listenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        std::cout << "bind()\n";

    // listen()
    retval = listen(listenSocket, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        std::cout << "listen()\n";



    //=================================================================================================================
    // 2. ���� ���� �ʱ�ȭ
    //=================================================================================================================
    
    // ���⼭ ���ǿ� ���õ� ��� �������� �ʱ�ȭ. ���� Ǯ�� �ʿ��ϴٸ� ���� �ڵ� ����
    maxConnections = maxSessionCount;
    sessions = new CSession[maxConnections];


    

    //=================================================================================================================
    // 3. ���� ������ �ʿ��� ������ �ε�
    //=================================================================================================================
    isServerMaintrenanceMode = true;

    InitializeCriticalSection(&cs_sessionID);

    // stack ������� ���� ID�� �迭 ������ ����( 0 ~ maxConnections ������ ���� �ε����� ��� )
    for (int i = maxConnections - 1; i >= 0; --i)
    {
        stSessionIndex.push(i);
    }




    //=================================================================================================================
    // 4. ������ Ǯ ����
    //=================================================================================================================
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

    return true;  // ���� �� true ��ȯ
}

void CGameServer::Stop(void)
{
    // ��� ������ ���� �� ���ҽ� ����

    // ===========================================================================
    // 1. ��� �����尡 ����Ǳ⸦ ���
    // ===========================================================================
    DWORD retVal = WaitForMultipleObjects(threadCnt, hThreads, TRUE, INFINITE);
    DWORD retError = GetLastError();

    // ===========================================================================
    // 2. ���ҽ� ����
    // ===========================================================================
    Release();

    // ===========================================================================
    // 3. �����尡 ���� ����Ǿ����� �˻�
    // ===========================================================================
    DWORD ExitCode;

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

void CGameServer::Release(void)
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
        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_FIND));

        // ��Ŷ ����
        CPacket packet;

        // ��� ���� ����
        PACKET_HEADER header;
        header.bySize = pPacket->GetDataSize();
        packet.PutData((char*)&header, sizeof(PACKET_HEADER));

        // ���ڷ� ���� ��Ŷ�� ������ ����
        packet.PutData(pPacket->GetFrontBufferPtr(), pPacket->GetDataSize());

        // sendQ�� ����
        pSession->sendQ.Enqueue(packet.GetFrontBufferPtr(), packet.GetDataSize());

        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_ENQ));
    }
    // �ƴ϶�� ��Ȱ��Ǹ鼭 �̻��� ���� ���� �� �ִ�.
    else
    {
        DebugBreak();
    }

    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_START_SENDPOST));

    SendPost(pSession);

    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_SENDPOST));
}

// ������ ��������� ���� ����� �޴� �Լ�. ���� �׽�Ʈ�� ���� ���� �ʿ��� �� �系 IP�� ������ �� �ֵ��� �ο��� �����ϴ� ���.
bool CGameServer::OnConnectionRequest(const std::string& ip, int port)
{
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
    UINT16 sessionIndex = stSessionIndex.top();
    stSessionIndex.pop();

    LeaveCriticalSection(&cs_sessionID);

    // �������� �� 1 ����
    InterlockedIncrement(&curSessionCnt);

    sessionID |= sessionIndex;
    sessions[sessionIndex].id = sessionID;

    return &sessions[sessionIndex];
}

void CGameServer::returnSession(CSession* pSession)
{
    // ���� close
    closesocket(pSession->sock);
    pSession->sock = INVALID_SOCKET;

    EnterCriticalSection(&cs_sessionID);

    // ���ڷ� ���� ������ ��ġ�� �迭 �ε����� ��ȯ
    stSessionIndex.push(static_cast<UINT16>(pSession->id));

    LeaveCriticalSection(&cs_sessionID);

    // �������� �� 1 ����
    InterlockedDecrement(&curSessionCnt);

    InterlockedIncrement(&disconnectedSessionCnt);

    // ������ ���������� ���Ŀ� ���������� ������ �����Ǿ����� �˸��� �ڵ尡 ��
    OnRelease(pSession->id);
}

void CGameServer::InitSessionInfo(CSession* pSession)
{
    // send/recv�� ������ ����ü �ʱ�ȭ
    ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
    ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));

    // ������ ���� IO Count �ο�, �ʱ�ȭ �� �ñ�� ���� ��ϵ� ���̴� 0���� ����
    pSession->IOCount = 0;

    // send�� 1ȸ �����ϱ� ���� flag ����
    pSession->sendFlag = 0;

    // ������ �ʱ�ȭ
    pSession->sendQ.ClearBuffer();
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
