#include "pch.h"
#include "ContentManager.h"
#include "Content.h"
#include "Packet.h"
#include "Session.h"
#include "Object.h"

ContentManager::ContentManager()
{
}

void ContentManager::InitWithIOCP(HANDLE port)
{
    hIOCP = port;
}

void ContentManager::registerContent(const int contentID, Content* c)
{
    entries.push_back({ contentID, c });
}

void ContentManager::transferSession(const int contentID, CSession* pSession)
{
    for (auto& e : entries) {
        if (e.contentID == contentID) {
            e.pContent->AddSession(pSession);
            break;
        }
    }
}

void ContentManager::removeSession(CSession* pSession)
{
    pSession->pObj->SetDead();
}

void ContentManager::Tick(void)
{
    for (auto& e : entries) {
        PostQueuedCompletionStatus(
            hIOCP,
            0,
            reinterpret_cast<ULONG_PTR>(e.pContent),
            nullptr
        );
    }
}