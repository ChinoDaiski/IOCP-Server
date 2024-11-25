
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

    //debugSessionIndexQueue.enqueue(std::make_pair(g_ID, index));

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
