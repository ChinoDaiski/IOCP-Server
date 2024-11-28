
#include "pch.h"
#include "Session.h"

CSession::CSession()
{
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