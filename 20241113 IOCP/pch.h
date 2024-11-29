#pragma once

#include <iostream>

//#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <Windows.h>
#pragma comment(lib, "Winmm.lib")	// timeBeginPeriod 사용을 위해 추가

#include <algorithm>

#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "CrashDump.h"
CCrashDump dump;
