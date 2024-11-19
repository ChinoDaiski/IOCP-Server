#pragma once

#include "RingBuffer.h"

// ���� ���� ������ ���� ����ü
class CSession
{
public:
    CSession() {
        InitializeCriticalSection(&cs);
    }
    ~CSession() {
        DeleteCriticalSection(&cs);
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

    CRITICAL_SECTION cs;
};