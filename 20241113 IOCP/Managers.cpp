#include "pch.h"
#include "Managers.h"

#include "ContentsManager.h"

void Managers::Init(HANDLE hIOCP)
{
	pContentMgr = new ContentManager(hIOCP);
}
