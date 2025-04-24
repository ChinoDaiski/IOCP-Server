
#include "pch.h"
#include "Session.h"

CSession::CSession()
{
    sock = 0;
    port = 0;
    memset(IP, '\0', sizeof(IP));
    recvQ.ClearBuffer();
    sendQ.ClearQueue();
    memset(&overlappedRecv, '\0', sizeof(overlappedRecv));
    memset(&overlappedSend, '\0', sizeof(overlappedSend));
    id = 0;
    IOCount = 0;
    sendFlag = 0;
    isAlive = 1;
}

CSession::~CSession()
{
}


void Logging(CSession* pSession, ACTION _action)
{
    DWORD curThreadID = GetCurrentThreadId();
    pSession->debugQueue.enqueue(std::make_pair(curThreadID, _action));
}

void Logging(CSession* pSession, UINT32 _action)
{
    Logging(pSession, (ACTION)_action);
}