
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
	// ���� �˻�
	auto iter = sessionMap.find(sessionID);

	// ������ ã�Ҵٸ�
	if (iter != sessionMap.end())
	{
		iter->second->sendQ.Enqueue(pPacket->GetBufferPtr(), pPacket->GetDataSize());
		pPacket->Clear();
	}
	else
	{
		DebugBreak();
	}

    // ������������ SendPost�� 
    SendPost(iter->second);
}

void ServerLib::RegisterSession(CSession* _pSession)
{
	sessionMap.emplace(g_ID, _pSession);
	++g_ID;

	// ������ accept �Լ� ���� -> ���ڰ� �ƴ� ��¥�� ���������� ���� ���� �� ��� ���.
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

	// ������ ���������� ���Ŀ� ���������� ������ �����Ǿ����� �˸��� �ڵ尡 ������. ���߿� �߰�
}

// sendQ�� �����͸� �ְ�, WSASend�� ȣ��
void ServerLib::SendPost(CSession* pSession)
{
    // ���� ���� ���� �� sendflag Ȯ����.
    // sending �ϴ� ���̶��
    if (InterlockedExchange(&pSession->sendFlag, 1) == 1)
        // �Ѿ
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
