#pragma once

#include "Packet.h"

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
