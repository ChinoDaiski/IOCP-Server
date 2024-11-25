
#include "pch.h"
#include "ServerLib.h"
//#include "Session.h"

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

    //debugSessionIndexQueue.enqueue(std::make_pair(g_ID, index));

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
