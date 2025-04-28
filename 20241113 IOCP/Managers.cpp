#include "pch.h"
#include "Managers.h"

#include "ContentManager.h"
#include "TimerManager.h"

Managers::Managers()
{
}

void Managers::Initialize()
{
	pContentMgr = new ContentManager();
	pTimerMgr = new TimerManager();
}