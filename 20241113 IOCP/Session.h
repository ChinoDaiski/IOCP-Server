#pragma once

#include "RingBuffer.h"
#include "CircularQueue.h"

enum class ACTION : UINT32
{
    ACTION_BEGIN,

    ACCEPT_AFTER_NEW,
    ACCEPT_CONNECT_IOCP,
    ACCEPT_REGISTER_SESSION,
    ACCEPT_RECVPOST,
    ACCEPT_DELETE_SESSION,
    ACCEPT_EXIT,
    
    WORKER_AFTER_GQCS,
    WORKER_ENTER_CS,
    WORKER_LEAVE_CS1,
    WORKER_LEAVE_CS2,
    WORKER_LEAVE_CS3,
    WORKER_RELEASE_SESSION,
    WORKER_DELETE_SESSION,

    WORKER_RECV_COMPLETION_NOTICE,
    WORKER_RECV_START_WHILE,
    WORKER_RECV_ONRECV_START,
    WORKER_RECV_ONRECV_AFTER,
    WORKER_RECV_EXIT_WHILE1,
    WORKER_RECV_EXIT_WHILE2,
    WORKER_RECV_RECVPOST_START,
    WORKER_RECV_RECVPOST_AFTER,
    
    WORKER_SEND_COMPLETION_NOTICE,
    WORKER_SEND_FLAG0,
    WORKER_SEND_HAS_DATA,
    WORKER_SEND_HAS_NODATA,
    WORKER_SEND_SENDPOST_START,
    WORKER_SEND_SENDPOST_AFTER,
    
    SENDPACKET_AFTER_FIND,
    SENDPACKET_AFTER_ENQ,
    SENDPACKET_START_SENDPOST,
    SENDPACKET_AFTER_SENDPOST,

    SERVERLIB_SENDPOST_SENDING,
    SERVERLIB_SENDPOST_TRY_SEND,


    IOCOUNT_0 = 100,
};


// ���� ���� ������ ���� ����ü
class CSession
{
public:
    CSession() {
        InitializeCriticalSection(&cs_session);
    }
    ~CSession() {
        DeleteCriticalSection(&cs_session);
    }

public:
    SOCKET sock;

    USHORT port;
    char IP[17];

    CRingBuffer recvQ;
    CRingBuffer sendQ;

    OVERLAPPED overlappedRecv;
    OVERLAPPED overlappedSend;

    UINT32 id;

    UINT32 IOCount;

    UINT32 sendFlag;    // ��Ƽ������ ȯ�濡�� interlockedexchange �Լ��� Sending���� �´��� Ȯ���ϱ� ���� flag ����

    UINT32 SendingCount;

    CRITICAL_SECTION cs_session;

    // ������ ID, �׼� ��ȣ�� pair�� ����
    CircularQueue<std::pair<DWORD, ACTION>> debugQueue;
};