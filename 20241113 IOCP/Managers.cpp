#include "pch.h"
#include "Managers.h"

#include "ContentManager.h"

Managers::Managers()
{
}

void Managers::Initialize()
{
	pContentMgr = new ContentManager();
}