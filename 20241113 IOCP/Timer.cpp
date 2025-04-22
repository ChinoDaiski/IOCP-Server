#include "pch.h"
#include "Timer.h"
#include <iostream>

CTimer::CTimer(void)
{
    processStartTime = std::chrono::steady_clock::now();
}

CTimer::~CTimer(void)
{
}

void CTimer::PrintElapsedTime(void)
{
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - processStartTime);

    int days = elapsed.count() / 86400;        // «œ∑Á = 86400√ 
    int hours = (elapsed.count() % 86400) / 3600;
    int minutes = (elapsed.count() % 3600) / 60;
    int seconds = elapsed.count() % 60;

    std::wcout << "Elapsed Time: "
        << days << " days  "
        << hours << " hours  "
        << minutes << " minutes  "
        << seconds << " seconds\n";
}