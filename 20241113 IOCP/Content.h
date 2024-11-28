#pragma once

#include "Packet.h"
#include "IServer.h"
#include "IContent.h"

#include <unordered_map>

class CPlayer
{
public:
    CPlayer();
    ~CPlayer();

public:
    UINT32 sessionID;
    UINT32 playerID;

private:
    static UINT32 g_ID;
};
