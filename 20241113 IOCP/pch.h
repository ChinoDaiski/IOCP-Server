#pragma once

#include <iostream>

//#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>

// �ּ� ���� ��ƿ (optional)
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

// MS ���� ���� Ȯ�� �Լ� ����
#include <mswsock.h>
#pragma comment(lib, "Mswsock.lib")

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <Windows.h>
#pragma comment(lib, "Winmm.lib")	// timeBeginPeriod ����� ���� �߰�

#include <algorithm>

#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <chrono>

#include "CrashDump.h"
CCrashDump dump;

#include "Defines.h"