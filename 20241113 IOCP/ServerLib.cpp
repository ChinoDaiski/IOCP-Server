
#include "pch.h"
#include "ServerLib.h"

#include <process.h>
#include "Protocol.h"


ServerLib::ServerLib(void)
    : g_ID{ 0 }, pIContent{ NULL }
{
}

ServerLib::~ServerLib(void)
{
}

void ServerLib::SendPacket(UINT64 sessionID, CPacket* pPacket)
{
    DWORD curThreadID = GetCurrentThreadId();

	// ���� �˻�
    UINT16 index = static_cast<UINT16>(sessionID);
    CSession* pSession = &sessionArr[index];

	// ������ ã�Ҵٸ�
    if (pSession->id == sessionID)
	{
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_FIND));

        CPacket packet;
        PACKET_HEADER header;
        header.bySize = pPacket->GetDataSize();
        packet.PutData((char*)&header, sizeof(PACKET_HEADER));
        packet.PutData(pPacket->GetFrontBufferPtr(), pPacket->GetDataSize());
        pSession->sendQ.Enqueue(packet.GetFrontBufferPtr(), packet.GetDataSize());

        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_ENQ));
	}
    // �ƴ϶�� ��Ȱ��Ǹ鼭 �̻��� ���� ���� �� �ִ�.
	else
	{
		DebugBreak();
	}

    pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_START_SENDPOST));

    SendPost(pSession);

    pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_SENDPOST));
}

CSession* ServerLib::FetchSession(void)
{
    UINT64 sessionID = 0;

    // ���� 6byte�� ���Ӹ��� 1�� �����ϴ� g_ID ���� �ִ´�.

    // ��ü 64bit�� ���� 48bit�� ���
    sessionID = g_ID;
    sessionID <<= 16;

    // ����� g_ID�� �ߺ� ������ ���� 1����
    g_ID++;

    UINT16 index = UINT16_MAX;
    // �迭�� for���� ���� ������� �ʴ� ������ ã��
    for (UINT16 i = 0; i < MAX_SESSION_CNT; ++i)
    {
        if (InterlockedExchange(&sessionArr[i].useFlag, 1) != 0)
            continue;

        index = i;
        break;
    }

    if (index == UINT16_MAX)
        return nullptr;

    sessionID |= index;

    sessionArr[index].id = sessionID;

    // ������ accept �Լ� ���� -> ���ڰ� �ƴ� ��¥�� ���������� ���� ���� �� ��� ���.
    //pIContent->OnAccept(g_ID);

    debugSessionIndexQueue.enqueue(std::make_pair(g_ID, index));

    return &sessionArr[index];
}

void ServerLib::DeleteSession(CSession* _pSession)
{
    // ���� close
    closesocket(_pSession->sock);
    _pSession->sock = INVALID_SOCKET;

    // useFlag�� 0���� ����
    UINT32 flag = InterlockedExchange(&_pSession->useFlag, 0);

    if (flag != 1)
    {
        UINT16 index = static_cast<UINT16>(_pSession->id);
        UINT64 gID = static_cast<UINT64>(_pSession->id >> 16);

        DebugBreak();
    }

	// ������ ���������� ���Ŀ� ���������� ������ �����Ǿ����� �˸��� �ڵ尡 ������. ���߿� �߰�.
    // ~ 
}

// sendQ�� �����͸� �ְ�, WSASend�� ȣ��
void ServerLib::SendPost(CSession* pSession)
{
    DWORD curThreadID = GetCurrentThreadId();

    // ���� ���� ���� �� sendflag Ȯ����.
    // sending �ϴ� ���̶��
    if (InterlockedExchange(&pSession->sendFlag, 1) == 1)
    {
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_SOMEONE_SENDING));
        // �Ѿ
        return;
    }

    int retval;
    int error;

    WSABUF sendBuf[2];
    int bufCnt = pSession->sendQ.makeWSASendBuf(sendBuf);

    // A �����尡 ����� Ȯ���ϰ� ���� interlock�� ȣ������ ���� ��Ȳ����, ���ؽ�Ʈ ����Ī. 
    // B �����尡 recvPost �����ϸ鼭 sendflag�� 1�� �ٲٰ� ����
    // C �����尡 send �Ϸ������� �޾� sendflag�� 0���� �ٲ�
    // A �����尡 �Ͼ�� interlock���� 0 ���� 1�� �ٲٰ� send ����. �ٵ� �̹� send �Ǿ���� ��Ȳ���� send 0�� ���� �� ����.
    // �̷��� �츮 �������� recv 0�϶� ������ �����ϱ⿡ �̸� �ذ��ϱ� ���ؼ� send�� 0�� ��Ȳ�� Ȯ���ؼ� ������ ���߿� ����.
    int sendLen = 0;
    for (int i = 0; i < bufCnt; ++i)
    {
        sendLen += sendBuf[i].len;
    }

    if (sendLen == 0)
    {
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_SEND0));

        // ���� WSASend�� ���ϴϱ� sendflag�� Ǯ������Ѵ�.
        InterlockedExchange(&pSession->sendFlag, 0);
        return;
    }


    pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_REAL_SEND));


    DWORD flags{};

    InterlockedIncrement(&pSession->IOCount);

    retval = WSASend(pSession->sock, sendBuf, bufCnt, NULL, flags, &pSession->overlappedSend, NULL);

    error = WSAGetLastError();
    if (retval == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            ;
        else
        {
            // �����ʿ��� ���� ����.
            if (error == WSAECONNRESET || error == WSAECONNABORTED)
            {
                InterlockedDecrement(&pSession->IOCount);
            }
            else
                DebugBreak();
        }
    }
}

void ServerLib::RecvPost(CSession* pSession)
{
    int retVal;
    int error;

    DWORD flags{};

    WSABUF recvBuf[2];
    int bufCnt = pSession->recvQ.makeWSARecvBuf(recvBuf);

    InterlockedIncrement(&pSession->IOCount);

    // �񵿱� recv ó���� ���� ���� recv�� �ɾ��. ���� �����ʹ� wsaBuf�� ����� �޸𸮿� ä���� ����.
    retVal = WSARecv(pSession->sock, recvBuf, bufCnt, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        InterlockedDecrement(&pSession->IOCount);

        // ���� �ʿ��� ���� ������ ����. 
        if (error == WSAECONNRESET || error == WSAECONNABORTED)
            return;

        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // �� �κ��� ������ϴµ� � ������ ������ �𸣴ϱ� Ȯ�� �������� �־.
        // ���߿� ��Ƽ������ ������ ������ ��ġ�� �ʴ� OnError ������ �񵿱� ��¹����� ��ü ����.
    }
}

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
    // 2. ������ Ǯ ����
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



    //=================================================================================================================
    // 3. ���� ���� �ʱ�ȭ
    //=================================================================================================================
    
    // ���⼭ ���ǿ� ���õ� ��� �������� �ʱ�ȭ. ���� Ǯ�� �ʿ��ϴٸ� ���� �ڵ� ����
    maxConnections = maxSessionCount;
    sessions = new CSession[maxConnections];
    ZeroMemory(sessions, sizeof(CSession*) * maxConnections);


    

    //=================================================================================================================
    // 4. ���� ������ �ʿ��� ������ �ε�
    //=================================================================================================================
    isServerMaintrenanceMode = true;

    InitializeCriticalSection(&cs_sessionID);

    // stack ������� ���� ID�� �迭 ������ ����
    for (UINT16 i = maxConnections; i > 0; --i)
    {
        stSessionIndex.push(i);
    }

    return true;  // ���� �� true ��ȯ
}

bool CGameServer::Disconnect(UINT32 sessionID)
{
    // ���� ���� ó��

    return true;
}

bool CGameServer::SendPacket(UINT32 sessionID, CPacket* pPacket)
{
    DWORD curThreadID = GetCurrentThreadId();

    // ���� �˻�
    UINT16 index = static_cast<UINT16>(sessionID);
    CSession* pSession = &sessions[index];

    // ������ ã�Ҵٸ�
    if (pSession->id == sessionID)
    {
        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_FIND));

        CPacket packet;
        PACKET_HEADER header;
        header.bySize = pPacket->GetDataSize();
        packet.PutData((char*)&header, sizeof(PACKET_HEADER));
        packet.PutData(pPacket->GetFrontBufferPtr(), pPacket->GetDataSize());
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

void CGameServer::OnAccept(UINT32 sessionID)
{
    // ���� �׽�Ʈ�� ���� ó�� ������ �� ������ data�� Ŭ�� ����.
    CPacket packet;
    UINT64 data = 0x7fffffffffffffff;
    packet.PutData((char*)&data, sizeof(data));

    // ��Ŷ�� ������ �Լ� 
    SendPacket(sessionID, &packet);
}

void CGameServer::OnRelease(UINT32 sessionID)
{
}

void CGameServer::OnRecv(UINT32 sessionID, CPacket* pPacket)
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

    sessionID |= sessionIndex;
    sessions[sessionIndex].id = sessionID;

    return &sessions[sessionIndex];
}

void CGameServer::returnSession(CSession* pSession)
{
    // ���� close
    closesocket(pSession->sock);
    pSession->sock = INVALID_SOCKET;

    // useFlag�� 0���� ����
    UINT32 flag = InterlockedExchange(&pSession->useFlag, 0);

    //if (flag != 1)
    //{
    //    UINT16 index = static_cast<UINT16>(_pSession->id);
    //    UINT64 gID = static_cast<UINT64>(_pSession->id >> 16);

    //    DebugBreak();
    //}

    // ������ ���������� ���Ŀ� ���������� ������ �����Ǿ����� �˸��� �ڵ尡 ��
    OnRelease(pSession->id);
}

CSession* CGameServer::InitSessionInfo(CSession* pSession)
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

    // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
    CreateIoCompletionPort((HANDLE)pSession->sock, hIOCP, (ULONG_PTR)pSession, 0);
    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_CONNECT_IOCP));

    // ���⼭ �������� ���� ���̳� �迭�� ������ ���

    // recv ó��, �� ����� �����ϰ�, ���Ŀ� WSARecv�� �ϳĸ�, ��� ���� Recv �Ϸ������� �� �� ����. �׷��� ����� ������
    RecvPost(pSession);
    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_RECVPOST));

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
