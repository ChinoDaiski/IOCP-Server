#pragma once

#include "CircularQueue.h"
#include "RingBuffer.h"

enum class ACTION : UINT32
{
    ACTION_BEGIN = 0,

    ACCEPT_AFTER_NEW = 10,
    ACCEPT_CONNECT_IOCP,
    ACCEPT_RECVPOST,
    ACCEPT_DELETE_SESSION,
    ACCEPT_EXIT,
    
    WORKER_AFTER_GQCS = 20,
    WORKER_ENTER_CS,
    WORKER_LEAVE_CS1,
    WORKER_LEAVE_CS2,
    WORKER_LEAVE_CS3,
    WORKER_RELEASE_SESSION,
    WORKER_DELETE_SESSION,

    WORKER_RECV_COMPLETION_NOTICE = 30,
    WORKER_RECV_START_WHILE,
    WORKER_RECV_ONRECV_START,
    WORKER_RECV_ONRECV_AFTER,
    WORKER_RECV_EXIT_WHILE1,
    WORKER_RECV_EXIT_WHILE2,
    WORKER_RECV_RECVPOST_START,
    WORKER_RECV_RECVPOST_AFTER,
    
    WORKER_SEND_COMPLETION_NOTICE = 40,
    WORKER_SEND_FLAG0,
    WORKER_SEND_HAS_DATA,
    WORKER_SEND_HAS_NODATA,
    WORKER_SEND_SENDPOST_START,
    WORKER_SEND_SENDPOST_AFTER,
    
    SENDPACKET_AFTER_FIND = 50,
    SENDPACKET_AFTER_ENQ,
    SENDPACKET_START_SENDPOST,
    SENDPACKET_AFTER_SENDPOST,

    SERVERLIB_SENDPOST_SOMEONE_SENDING,
    SERVERLIB_SENDPOST_REAL_SEND,
    SERVERLIB_SENDPOST_SEND0,

    WORKER_CALL_DELETE_SESSION1,
    WORKER_CALL_DELETE_SESSION2,
    WORKER_CALL_DELETE_SESSION3,

    WORKER_RETVAL_FALSE,
    WORKER_RECV_LEN0,

    IOCOUNT_0 = 100,

    RECV_QUEUE_USESIZE = 200,
};

// ���� ���� ������ ���� ����ü
class CSession
{
public:
    CSession();
    ~CSession();

public:
    SOCKET sock;

    USHORT port;
    char IP[17];

    CRingBuffer recvQ;
    CRingBuffer sendQ;

    OVERLAPPED overlappedRecv;
    OVERLAPPED overlappedSend;

    UINT64 id;

    UINT32 IOCount;

    UINT32 sendFlag;    // ��Ƽ������ ȯ�濡�� interlockedexchange �Լ��� Sending���� �´��� Ȯ���ϱ� ���� flag ����

    UINT32 useFlag; // ���� �� lock�� �����ϱ� ���� �켱 ������ �迭�� �̸� �����ΰ�, bool ������ ��뿩�θ� Ȯ���ϴ� ������� ������ ��Ȱ���ϱ�� �ߴ�.
                    // �̸� ���� ���� ����. �ʱⰪ�� ������� �ʾ����� 0, ����ϸ� 1�� �ٲ۴�.

    // ������ ID, �׼� ��ȣ�� pair�� ����
    CircularQueue<std::pair<DWORD, ACTION>> debugQueue;
};