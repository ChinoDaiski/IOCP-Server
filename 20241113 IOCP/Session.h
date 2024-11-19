#pragma once

#include "RingBuffer.h"

// 소켓 정보 저장을 위한 구조체
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

    UINT32 sendFlag;    // 멀티스레드 환경에서 interlockedexchange 함수로 Sending중이 맞는지 확인하기 위한 flag 변수

    UINT32 SendingCount;

    CRITICAL_SECTION cs;
};