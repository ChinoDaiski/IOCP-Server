#pragma once

#include "CircularQueue.h"
#include "RingBuffer.h"

enum class ACTION : UINT32
{
    ACTION_BEGIN = 0,

    ACCEPT_AFTER_FETCH = 10,
    ACCEPT_AFTER_INIT,
    ACCEPT_AFTER_NEW,
    ACCEPT_ON_ACCEPT,
    ACCEPT_RECVPOST,
    ACCEPT_DELETE_SESSION,
    ACCEPT_EXIT,
    
    WORKER_AFTER_GQCS = 20,
    WORKER_ENTER_CS,
    WORKER_LEAVE_CS1,
    WORKER_LEAVE_CS2,
    WORKER_LEAVE_CS3,
    WORKER_RELEASE_SESSION,

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
    WORKER_SEND_NODATA,
    WORKER_SEND_SENDPOST_START,
    WORKER_SEND_SENDPOST_AFTER,
    
    SENDPACKET_AFTER_FIND = 50,
    SENDPACKET_AFTER_ENQ,
    SENDPACKET_START_SENDPOST,
    SENDPACKET_AFTER_SENDPOST,

    SERVERLIB_SENDPOST_SOMEONE_SENDING,
    SERVERLIB_SENDPOST_REAL_SEND,
    SERVERLIB_SENDPOST_SEND0,

    SENDPOST_WSASEND_FAIL_NOPENDING = 60,
    SENDPOST_WSARECV_FAIL_NOPENDING,

    WORKER_CALL_DELETE_SESSION1,
    WORKER_CALL_DELETE_SESSION2,
    WORKER_CALL_DELETE_SESSION3,

    WORKER_RETVAL_FALSE,
    WORKER_RECV_LEN0,

    IOCOUNT_0 = 100,

    RECV_QUEUE_USESIZE = 200,

    SEND_QUEUE_SENDQ_READPOS = 300,
    SEND_QUEUE_SENDQ_WRITEPOS = 400,

    SEND_QUEUE_SENDCOMPELETE_SIZE = 1000,
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
    char IP[16];    // IPv4�� �ٷ�⿡ 16byte(15 + 1) ���

    CRingBuffer recvQ;
    CPacketQueue sendQ;

    OVERLAPPED overlappedRecv;
    OVERLAPPED overlappedSend;

    UINT64 id;

    UINT32 IOCount = 0;

    UINT32 sendFlag;    // ��Ƽ������ ȯ�濡�� interlockedexchange �Լ��� Sending���� �´��� Ȯ���ϱ� ���� flag ����
    
    UINT32 isAlive;     // ��� �ִ��� ����, ������ true�̳� ���������� ������ �ʿ��� ���� ���� ��� false�� �ǰ�, �̸� Ȯ���ؼ� IO Count�� 0�� ��� �����ϵ��� �����Ѵ�.

    // ������ ID, �׼� ��ȣ�� pair�� ����
    CircularQueue<std::pair<DWORD, ACTION>> debugQueue;
};

void Logging(CSession* pSession, ACTION _action);
void Logging(CSession* pSession, UINT32 _action);