
#include "pch.h"
#include "ServerLib.h"
#include "Session.h"

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

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

	// ���� �˻�
    EnterCriticalSection(&cs_sessionMap);
	auto iter = sessionMap.find(sessionID);
    LeaveCriticalSection(&cs_sessionMap);

	// ������ ã�Ҵٸ�
	if (iter != sessionMap.end())
	{
        iter->second->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_FIND));

        CPacket packet;
        PACKET_HEADER header;
        header.bySize = pPacket->GetDataSize();
        packet.PutData((char*)&header, sizeof(PACKET_HEADER));
        packet.PutData(pPacket->GetBufferPtr(), pPacket->GetDataSize());
        iter->second->sendQ.Enqueue(packet.GetBufferPtr(), packet.GetDataSize());

        iter->second->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_ENQ));
	}
	else
	{
		DebugBreak();
	}

    iter->second->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_START_SENDPOST));

    // ������������ SendPost�� 
    SendPost(iter->second);

    iter->second->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_SENDPOST));
}

void ServerLib::RegisterSession(CSession* _pSession)
{
    EnterCriticalSection(&cs_sessionMap);
	sessionMap.emplace(g_ID, _pSession);
    LeaveCriticalSection(&cs_sessionMap);
	++g_ID;

	// ������ accept �Լ� ���� -> ���ڰ� �ƴ� ��¥�� ���������� ���� ���� �� ��� ���.
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

	// ������ ���������� ���Ŀ� ���������� ������ �����Ǿ����� �˸��� �ڵ尡 ������. ���߿� �߰�
}

// sendQ�� �����͸� �ְ�, WSASend�� ȣ��
void ServerLib::SendPost(CSession* pSession)
{
    DWORD curThreadID = GetCurrentThreadId();

    // ���� ���� ���� �� sendflag Ȯ����.
    // sending �ϴ� ���̶��
    if (InterlockedExchange(&pSession->sendFlag, 1) == 1)
    {
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_SENDING));
        // �Ѿ
        return;
    }

    pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_TRY_SEND));

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

    // send�ϴ� �����Ͱ� 0�̶��
    if ((wsaBuf[0].len + wsaBuf[1].len) == 0)
        // return �� ��.
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
        {
            // �����ʿ��� ���� ����.
            if (error == WSAECONNRESET)
            {
                InterlockedDecrement(&pSession->IOCount);
            }
            else
                DebugBreak();
        }
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

    // �񵿱� recv ó���� ���� ���� recv�� �ɾ��. ���� �����ʹ� wsaBuf�� ����� �޸𸮿� ä���� ����.
    retVal = WSARecv(pSession->sock, recvBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        InterlockedDecrement(&pSession->IOCount);
        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // �� �κ��� ������ϴµ� � ������ ������ �𸣴ϱ� Ȯ�� �������� �־.
        // ���߿� ��Ƽ������ ������ ������ ��ġ�� �ʴ� OnError ������ �񵿱� ��¹����� ��ü ����.
    }
}
