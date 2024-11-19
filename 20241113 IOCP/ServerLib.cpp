
#include "pch.h"
#include "ServerLib.h"
#include "Session.h"

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

ServerLib::ServerLib(void)
	: g_ID{ 0 }
{
}

void ServerLib::SendPacket(int sessionID, CPacket* pPacket)
{
	// 세션 검색
	auto iter = sessionMap.find(sessionID);

	// 세션을 찾았다면
	if (iter != sessionMap.end())
	{
		iter->second->sendQ.Enqueue(pPacket->GetBufferPtr(), pPacket->GetDataSize());
		pPacket->Clear();
	}
	else
	{
		DebugBreak();
	}

    // 컨텐츠이지만 SendPost를 
    SendPost(iter->second);
}

void ServerLib::RegisterSession(CSession* _pSession)
{
	sessionMap.emplace(g_ID, _pSession);
	++g_ID;

	// 콘텐츠 accept 함수 생성 -> 에코가 아닌 진짜로 콘텐츠에서 뭔가 만들 때 등록 요망.
	//pIContent->OnAccept(g_ID);
}

void ServerLib::DeleteSession(CSession* _pSession)
{
	auto iter = sessionMap.find(_pSession->id);

	if (iter != sessionMap.end())
	{
		sessionMap.erase(iter);
		delete _pSession;
	}

	// 세션을 삭제했으니 이후에 콘텐츠에게 세션이 삭제되었음을 알린느 코드가 들어가야함. 나중에 추가
}

// sendQ에 데이터를 넣고, WSASend를 호출
void ServerLib::SendPost(CSession* pSession)
{
    // 보낼 것이 있을 때 sendflag 확인함.
    // sending 하는 중이라면
    if (InterlockedExchange(&pSession->sendFlag, 1) == 1)
        // 넘어감
        return;

    UINT32 a = InterlockedIncrement(&pSession->SendingCount);

    if (a == 2)
        DebugBreak();

    int retval;
    int error;

    WSABUF wsaBuf[2];
    wsaBuf[0].buf = pSession->sendQ.GetFrontBufferPtr();
    wsaBuf[0].len = pSession->sendQ.DirectDequeueSize();

    wsaBuf[1].buf = pSession->sendQ.GetBufferPtr();

    int transferredDataLen = pSession->sendQ.GetUseSize();

    // 보내려는 데이터의 길이가 sendQ에서 뺄 수 있는 데이터의 길이보다 크다면
    if (transferredDataLen > pSession->sendQ.DirectDequeueSize())
    {
        wsaBuf[1].len = transferredDataLen - wsaBuf[0].len;
    }
    // 보내려는 데이터의 길이가 sendQ에서 뺄 수 있는 데이터의 길이와 비교했을 때 작거나 같다면
    else
    {
        wsaBuf[1].len = 0;
    }

    // send하는 데이터가 0이라면
    if ((wsaBuf[0].len + wsaBuf[1].len) == 0)
        // return 할 것.
        return;

    DWORD flags{};

    InterlockedIncrement(&pSession->IOCount);

    retval = WSASend(pSession->sock, wsaBuf, 2, NULL, flags, &pSession->overlappedSend, NULL);

    error = WSAGetLastError();
    if (retval == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            ;
        else
            DebugBreak();
    }

    InterlockedDecrement(&pSession->SendingCount);
}

void ServerLib::RecvPost(CSession* pSession)
{
    int retVal;
    int error;

    DWORD flags{};

    WSABUF recvBuf[2];
    recvBuf[0].buf = pSession->recvQ.GetFrontBufferPtr();
    recvBuf[0].len = pSession->recvQ.DirectEnqueueSize();
    recvBuf[1].buf = pSession->recvQ.GetBufferPtr();
    recvBuf[1].len = pSession->recvQ.GetBufferCapacity() - pSession->recvQ.DirectEnqueueSize();

    InterlockedIncrement(&pSession->IOCount);

    // 비동기 recv 처리를 위해 먼저 recv를 걸어둠. 받은 데이터는 wsaBuf에 등록한 메모리에 채워질 예정.
    retVal = WSARecv(pSession->sock, recvBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        InterlockedDecrement(&pSession->IOCount);
        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // 이 부분은 없어야하는데 어떤 오류가 생길지 모르니깐 확인 차원에서 넣어봄.
        // 나중엔 멀티스레드 로직에 영향을 미치지 않는 OnError 등으로 비동기 출력문으로 교체 예정.
    }
}
