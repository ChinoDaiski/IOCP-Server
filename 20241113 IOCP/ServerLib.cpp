
#include "pch.h"
#include "ServerLib.h"
#include "Session.h"

#include "Protocol.h"

ServerLib::ServerLib(void)
    : g_ID{ 0 }, pIContent{ NULL }
{
    InitializeCriticalSection(&cs_sessionMap);
}

ServerLib::~ServerLib(void)
{
    DeleteCriticalSection(&cs_sessionMap);
}

void ServerLib::SendPacket(int sessionID, CPacket* pPacket)
{
    DWORD curThreadID = GetCurrentThreadId();

	// 세션 검색
    EnterCriticalSection(&cs_sessionMap);
	auto iter = sessionMap.find(sessionID);
    LeaveCriticalSection(&cs_sessionMap);

	// 세션을 찾았다면
	if (iter != sessionMap.end())
	{
        iter->second->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_FIND));

        CPacket packet;
        PACKET_HEADER header;
        header.bySize = pPacket->GetDataSize();
        packet.PutData((char*)&header, sizeof(PACKET_HEADER));
        packet.PutData(pPacket->GetFrontBufferPtr(), pPacket->GetDataSize());
        iter->second->sendQ.Enqueue(packet.GetFrontBufferPtr(), packet.GetDataSize());

        iter->second->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_ENQ));
	}
	else
	{
		DebugBreak();
	}

    iter->second->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_START_SENDPOST));

    // 컨텐츠이지만 SendPost를 
    SendPost(iter->second);

    iter->second->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_SENDPOST));
}

void ServerLib::RegisterSession(CSession* _pSession)
{
    EnterCriticalSection(&cs_sessionMap);
	sessionMap.emplace(g_ID, _pSession);
    LeaveCriticalSection(&cs_sessionMap);
	++g_ID;

	// 콘텐츠 accept 함수 생성 -> 에코가 아닌 진짜로 콘텐츠에서 뭔가 만들 때 등록 요망.
	//pIContent->OnAccept(g_ID);
}

void ServerLib::DeleteSession(CSession* _pSession)
{
    EnterCriticalSection(&cs_sessionMap);
	auto iter = sessionMap.find(_pSession->id);
    LeaveCriticalSection(&cs_sessionMap);

	if (iter != sessionMap.end())
	{
        EnterCriticalSection(&cs_sessionMap);
		sessionMap.erase(iter);
        LeaveCriticalSection(&cs_sessionMap);
		delete _pSession;
	}

	// 세션을 삭제했으니 이후에 콘텐츠에게 세션이 삭제되었음을 알린느 코드가 들어가야함. 나중에 추가
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

    //WSABUF wsaBuf[2];
    //wsaBuf[0].buf = pSession->sendQ.GetFrontBufferPtr();
    //wsaBuf[0].len = pSession->sendQ.DirectDequeueSize();

    //wsaBuf[1].buf = pSession->sendQ.GetBufferPtr();

    //int transferredDataLen = pSession->sendQ.GetUseSize();

    //// 보내려는 데이터의 길이가 sendQ에서 뺄 수 있는 데이터의 길이보다 크다면
    //if (transferredDataLen > pSession->sendQ.DirectDequeueSize())
    //{
    //    wsaBuf[1].len = transferredDataLen - wsaBuf[0].len;
    //}
    //// 보내려는 데이터의 길이가 sendQ에서 뺄 수 있는 데이터의 길이와 비교했을 때 작거나 같다면
    //else
    //{
    //    wsaBuf[1].len = 0;
    //}

    //// send하는 데이터가 0이라면
    //if ((wsaBuf[0].len + wsaBuf[1].len) == 0)
    //    // return 할 것.
    //    return;


    //if (memcmp(pSession->testQ.GetFrontBufferPtr(), wsaBuf[0].buf, wsaBuf[0].len) == 0)
    //{
    //    pSession->testQ.MoveFront(wsaBuf[0].len);
    //}
    //else
    //{
    //    DebugBreak();
    //}

    //if (memcmp(pSession->testQ.GetFrontBufferPtr(), wsaBuf[1].buf, wsaBuf[1].len) == 0)
    //{
    //    pSession->testQ.MoveFront(wsaBuf[1].len);
    //}
    //else
    //{
    //    DebugBreak();
    //}

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
    pSession->doSend = true;

    error = WSAGetLastError();
    if (retval == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            ;
        else
        {
            pSession->doSend = false;

            // 상대방쪽에서 먼저 끊음.
            if (error == WSAECONNRESET)
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

    //WSABUF recvBuf[2];
    //recvBuf[0].buf = pSession->recvQ.GetFrontBufferPtr();
    //recvBuf[0].len = pSession->recvQ.DirectEnqueueSize();
    //recvBuf[1].buf = pSession->recvQ.GetBufferPtr();
    //recvBuf[1].len = pSession->recvQ.GetBufferCapacity() - pSession->recvQ.DirectEnqueueSize();

    InterlockedIncrement(&pSession->IOCount);

    // 비동기 recv 처리를 위해 먼저 recv를 걸어둠. 받은 데이터는 wsaBuf에 등록한 메모리에 채워질 예정.
    retVal = WSARecv(pSession->sock, recvBuf, bufCnt, NULL, &flags, &pSession->overlappedRecv, NULL);
    pSession->doRecv = true;

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        pSession->doRecv = false;
        InterlockedDecrement(&pSession->IOCount);
        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // 이 부분은 없어야하는데 어떤 오류가 생길지 모르니깐 확인 차원에서 넣어봄.
        // 나중엔 멀티스레드 로직에 영향을 미치지 않는 OnError 등으로 비동기 출력문으로 교체 예정.
    }
}
