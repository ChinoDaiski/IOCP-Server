
#include "pch.h"
#include "Timer.h"

Timer::Timer(void)
{
    processStartTime = std::chrono::steady_clock::now();
}

Timer::~Timer(void)
{
}

void Timer::PrintElapsedTime(void)
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
        << seconds << " seconds";
}