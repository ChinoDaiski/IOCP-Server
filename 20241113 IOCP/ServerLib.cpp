
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
        packet.PutData(pPacket->GetFrontBufferPtr(), pPacket->GetDataSize());
        iter->second->sendQ.Enqueue(packet.GetFrontBufferPtr(), packet.GetDataSize());

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


    if (memcmp(pSession->testQ.GetFrontBufferPtr(), wsaBuf[0].buf, wsaBuf[0].len) == 0)
    {
        pSession->testQ.MoveFront(wsaBuf[0].len);
    }
    else
    {
        DebugBreak();
    }

    if (memcmp(pSession->testQ.GetFrontBufferPtr(), wsaBuf[1].buf, wsaBuf[1].len) == 0)
    {
        pSession->testQ.MoveFront(wsaBuf[1].len);
    }
    else
    {
        DebugBreak();
    }



    DWORD flags{};

    InterlockedIncrement(&pSession->IOCount);

    retval = WSASend(pSession->sock, wsaBuf, 2, NULL, flags, &pSession->overlappedSend, NULL);
    pSession->doSend = true;

    error = WSAGetLastError();
    if (retval == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            ;
        else
        {
            pSession->doSend = false;

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

    // wsaBuf�� ����� �Լ��� ����
    WSABUF recvBuf[2];
    recvBuf[0].buf = pSession->recvQ.GetFrontBufferPtr();
    recvBuf[0].len = pSession->recvQ.DirectEnqueueSize();
    recvBuf[1].buf = pSession->recvQ.GetBufferPtr();
    recvBuf[1].len = pSession->recvQ.GetBufferCapacity() - pSession->recvQ.DirectEnqueueSize();

    InterlockedIncrement(&pSession->IOCount);

    // �񵿱� recv ó���� ���� ���� recv�� �ɾ��. ���� �����ʹ� wsaBuf�� ����� �޸𸮿� ä���� ����.
    retVal = WSARecv(pSession->sock, recvBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL); 
    pSession->doRecv = true;

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        pSession->doRecv = false;
        InterlockedDecrement(&pSession->IOCount);
        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // �� �κ��� ������ϴµ� � ������ ������ �𸣴ϱ� Ȯ�� �������� �־.
        // ���߿� ��Ƽ������ ������ ������ ��ġ�� �ʴ� OnError ������ �񵿱� ��¹����� ��ü ����.
    }
}
