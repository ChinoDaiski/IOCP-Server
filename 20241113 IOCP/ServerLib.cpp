
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

	// 세션 검색
    UINT16 index = static_cast<UINT16>(sessionID);
    CSession* pSession = &sessionArr[index];

	// 세션을 찾았다면
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
    // 아니라면 재활용되면서 이상한 값이 나올 수 있다.
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

    // 상위 6byte는 접속마다 1씩 증가하는 g_ID 값을 넣는다.

    // 전체 64bit중 상위 48bit를 사용
    sessionID = g_ID;
    sessionID <<= 16;

    // 사용한 g_ID는 중복 방지를 위해 1증가
    g_ID++;

    UINT16 index = UINT16_MAX;
    // 배열을 for문을 돌며 사용하지 않는 세션을 찾음
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

    // 콘텐츠 accept 함수 생성 -> 에코가 아닌 진짜로 콘텐츠에서 뭔가 만들 때 등록 요망.
    //pIContent->OnAccept(g_ID);

    debugSessionIndexQueue.enqueue(std::make_pair(g_ID, index));

    return &sessionArr[index];
}

void ServerLib::DeleteSession(CSession* _pSession)
{
    // 소켓 close
    closesocket(_pSession->sock);
    _pSession->sock = INVALID_SOCKET;

    // useFlag를 0으로 변경
    UINT32 flag = InterlockedExchange(&_pSession->useFlag, 0);

    if (flag != 1)
    {
        UINT16 index = static_cast<UINT16>(_pSession->id);
        UINT64 gID = static_cast<UINT64>(_pSession->id >> 16);

        DebugBreak();
    }

	// 세션을 삭제했으니 이후에 콘텐츠에게 세션이 삭제되었음을 알리는 코드가 들어가야함. 나중에 추가.
    // ~ 
}

// sendQ에 데이터를 넣고, WSASend를 호출
void ServerLib::SendPost(CSession* pSession)
{
    DWORD curThreadID = GetCurrentThreadId();

    // 보낼 것이 있을 때 sendflag 확인함.
    // sending 하는 중이라면
    if (InterlockedExchange(&pSession->sendFlag, 1) == 1)
    {
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_SOMEONE_SENDING));
        // 넘어감
        return;
    }

    int retval;
    int error;

    WSABUF sendBuf[2];
    int bufCnt = pSession->sendQ.makeWSASendBuf(sendBuf);

    // A 스레드가 사이즈를 확인하고 아직 interlock을 호출하지 않은 상황에서, 컨텍스트 스위칭. 
    // B 스레드가 recvPost 진행하면서 sendflag를 1로 바꾸고 진행
    // C 스레드가 send 완료통지를 받아 sendflag를 0으로 바꿈
    // A 스레드가 일어나서 interlock으로 0 에서 1로 바꾸고 send 진행. 근데 이미 send 되어버린 상황에서 send 0이 나올 수 있음.
    // 이러면 우리 구조에서 recv 0일때 소켓을 제거하기에 이를 해결하기 위해서 send시 0인 상황을 확인해서 진행을 도중에 멈춤.
    int sendLen = 0;
    for (int i = 0; i < bufCnt; ++i)
    {
        sendLen += sendBuf[i].len;
    }

    if (sendLen == 0)
    {
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_SEND0));

        // 실제 WSASend를 안하니깐 sendflag를 풀어줘야한다.
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
            // 상대방쪽에서 먼저 끊음.
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

    // 비동기 recv 처리를 위해 먼저 recv를 걸어둠. 받은 데이터는 wsaBuf에 등록한 메모리에 채워질 예정.
    retVal = WSARecv(pSession->sock, recvBuf, bufCnt, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        InterlockedDecrement(&pSession->IOCount);

        // 상대방 쪽에서 먼저 연결을 끊음. 
        if (error == WSAECONNRESET || error == WSAECONNABORTED)
            return;

        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // 이 부분은 없어야하는데 어떤 오류가 생길지 모르니깐 확인 차원에서 넣어봄.
        // 나중엔 멀티스레드 로직에 영향을 미치지 않는 OnError 등으로 비동기 출력문으로 교체 예정.
    }
}

bool CGameServer::Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, UINT16 maxSessionCount)
{
    //=================================================================================================================
    // 1. 서버 소켓 초기화 및 IOCP 연결
    //=================================================================================================================

    int retval;

    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // CPU 개수 확인
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    // int ProcessorNum = si.dwNumberOfProcessors;
    // 프로세서 갯수 미만으로 IOCP Running 스레드의 갯수를 제한하면서 테스트 해볼 것.

    // 입출력 완료 포트 생성
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


    // sendbuf 크기 설정 (송신 버퍼의 크기를 0으로 설정해 중간 단계 버퍼링 없이 메모리에 직접 direct I/O를 유도)
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
    // 2. 스레드 풀 생성
    //=================================================================================================================
    threadCnt = workerThreadCount;
    hThreads = new HANDLE[threadCnt + 1];

    DWORD dwThreadID;

    // Accept Thread 호출
    hThreads[0] = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, (unsigned int*)&dwThreadID);

    // Worker Threads 호출
    for (UINT8 i = 0; i < workerThreadCount; ++i)
    {
        hThreads[i + 1] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, (unsigned int*)&dwThreadID);
    }



    //=================================================================================================================
    // 3. 세션 관리 초기화
    //=================================================================================================================
    
    // 여기서 세션에 관련된 모든 정보들을 초기화. 만약 풀이 필요하다면 관련 코드 삽입
    maxConnections = maxSessionCount;
    sessions = new CSession[maxConnections];
    ZeroMemory(sessions, sizeof(CSession*) * maxConnections);


    

    //=================================================================================================================
    // 4. 서버 가동에 필요한 정보를 로드
    //=================================================================================================================
    isServerMaintrenanceMode = true;

    InitializeCriticalSection(&cs_sessionID);

    // stack 방식으로 세션 ID의 배열 정보를 저장
    for (UINT16 i = maxConnections; i > 0; --i)
    {
        stSessionIndex.push(i);
    }

    return true;  // 성공 시 true 반환
}

bool CGameServer::Disconnect(UINT32 sessionID)
{
    // 세션 종료 처리

    return true;
}

bool CGameServer::SendPacket(UINT32 sessionID, CPacket* pPacket)
{
    DWORD curThreadID = GetCurrentThreadId();

    // 세션 검색
    UINT16 index = static_cast<UINT16>(sessionID);
    CSession* pSession = &sessions[index];

    // 세션을 찾았다면
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
    // 아니라면 재활용되면서 이상한 값이 나올 수 있다.
    else
    {
        DebugBreak();
    }

    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_START_SENDPOST));

    SendPost(pSession);

    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_SENDPOST));
}

// 세션이 만들어지기 전에 허락을 받는 함수. 서버 테스트나 점검 등이 필요할 때 사내 IP만 접속할 수 있도록 인원을 제한하는 방식.
bool CGameServer::OnConnectionRequest(const std::string& ip, int port)
{
    // 인자로 접속하는 클라이언트의 IP와 PORT를 넘기고, 이 함수 안에서 특정 IP의 접속에 대한 것을 처리한다.

    // 처음 서버에 접속했을 경우
    if (isServerMaintrenanceMode)
    {
        // 접속한 IP가 allowedIPs에 등록된 IP일 경우 접속을 허용, 아닐 경우 차단
        const auto& iter = allowedIPs.find(ip);

        if (iter != allowedIPs.end())
            return true;
    }
    // 서버 점검이 끝난 경우
    else
    {
        // 접속한 IP가 blockedIPs에 등록된 IP가 아닐 경우 접속을 허용, 아닐 경우 차단
        const auto& iter = blockedIPs.find(ip);

        if (iter == blockedIPs.end())
            return true;
    }

    return false;
}

void CGameServer::OnAccept(UINT32 sessionID)
{
    // 더미 테스트를 위해 처음 접속할 때 정해진 data를 클라에 전송.
    CPacket packet;
    UINT64 data = 0x7fffffffffffffff;
    packet.PutData((char*)&data, sizeof(data));

    // 패킷을 보내는 함수 
    SendPacket(sessionID, &packet);
}

void CGameServer::OnRelease(UINT32 sessionID)
{
}

void CGameServer::OnRecv(UINT32 sessionID, CPacket* pPacket)
{
    // 여기서 컨텐츠 처리가 들어감.
    // ~ 컨텐츠 ~
    // 컨텐츠 처리를 하면서 패킷을 만들어 SendPacket를 호출
    // sendQ에 데이터를 삽입. + sending 여부를 판단해 sendQ에 넣은 데이터를 보낼 수 있다면 보낸다.

    SendPacket(sessionID, pPacket);
}

CSession* CGameServer::FetchSession(void)
{
    UINT64 sessionID = 0;

    // 상위 6byte는 접속마다 1씩 증가하는 uniqueSessionID 값을 넣는다.

    // 전체 64bit중 상위 48bit를 사용
    sessionID = uniqueSessionID;
    sessionID <<= 16;

    // 사용한 uniqueSessionID는 중복 방지를 위해 1증가
    uniqueSessionID++;

    EnterCriticalSection(&cs_sessionID);

    // 사용하지 않는 배열 인덱스를 찾음
    UINT16 sessionIndex = stSessionIndex.top();
    stSessionIndex.pop();

    LeaveCriticalSection(&cs_sessionID);

    sessionID |= sessionIndex;
    sessions[sessionIndex].id = sessionID;

    return &sessions[sessionIndex];
}

void CGameServer::returnSession(CSession* pSession)
{
    // 소켓 close
    closesocket(pSession->sock);
    pSession->sock = INVALID_SOCKET;

    // useFlag를 0으로 변경
    UINT32 flag = InterlockedExchange(&pSession->useFlag, 0);

    //if (flag != 1)
    //{
    //    UINT16 index = static_cast<UINT16>(_pSession->id);
    //    UINT64 gID = static_cast<UINT64>(_pSession->id >> 16);

    //    DebugBreak();
    //}

    // 세션을 삭제했으니 이후에 콘텐츠에게 세션이 삭제되었음을 알리는 코드가 들어감
    OnRelease(pSession->id);
}

CSession* CGameServer::InitSessionInfo(CSession* pSession)
{
    // send/recv용 오버랩 구조체 초기화
    ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
    ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));

    // 삭제를 위한 IO Count 부여, 초기화 할 시기로 아직 등록도 전이니 0으로 설정
    pSession->IOCount = 0;

    // send를 1회 제한하기 위한 flag 변수
    pSession->sendFlag = 0;

    // 링버퍼 초기화
    pSession->sendQ.ClearBuffer();
    pSession->recvQ.ClearBuffer();

    // 소켓과 입출력 완료 포트 연결 - 새로 연견될 소켓을 key를 해당 소켓과 연결된 세션 구조체의 포인터로 하여 IOCP와 연결한다.
    CreateIoCompletionPort((HANDLE)pSession->sock, hIOCP, (ULONG_PTR)pSession, 0);
    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_CONNECT_IOCP));

    // 여기서 전역적인 세션 맵이나 배열에 세션을 등록

    // recv 처리, 왜 등록을 먼저하고, 이후에 WSARecv를 하냐면, 등록 전에 Recv 완료통지가 올 수 있음. 그래서 등록을 먼저함
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
