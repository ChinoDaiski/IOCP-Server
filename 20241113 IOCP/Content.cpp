
#include "pch.h"

#include "Content.h"
#include "Session.h"

UINT32 CPlayer::g_ID = 0;

CPlayer::CPlayer()
{
	playerID = g_ID;
	g_ID++;
}

CPlayer::~CPlayer()
{
}
