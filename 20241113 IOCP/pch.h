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
#include <algorithm>
