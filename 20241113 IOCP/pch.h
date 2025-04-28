#pragma once

#include <iostream>

//#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>

// 주소 관련 유틸 (optional)
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

// MS 전용 소켓 확장 함수 선언
#include <mswsock.h>
#pragma comment(lib, "Mswsock.lib")

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

#include <chrono>

#include "CrashDump.h"
CCrashDump dump;

#include "Defines.h"