#include "pch.h"
#include "TimerManager.h"
#include <iostream>

void TimerManager::InitTimer(int targetFPS) noexcept
{
    fps = targetFPS;
    interval = std::chrono::milliseconds(1000 / fps);
    lastTickTime = Clock::now();
    startTime = lastTickTime;
    processStartTime = lastTickTime;
    frameCount = 0;
    totalFrame = 0;
}

void TimerManager::ChangeInterval(int targetFPS) noexcept
{
    fps = targetFPS;
    interval = std::chrono::milliseconds(1000 / fps);
}

int TimerManager::CheckFrame() noexcept
{
    auto now = Clock::now();

    TimePoint nextTime = lastTickTime + interval;
    if (now < nextTime) {
        // ���� tick���� ���� �ð�(ms)
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(nextTime - now).count());
    }

    // ������ ����: lastTickTime ����
    lastTickTime += interval;
    ++frameCount;
    ++totalFrame;

    // 1�ʸ��� FPS �α� ���
    if (now - startTime >= std::chrono::seconds(1)) {
        std::cout << "\nFPS: " << frameCount << "\n\n";
        startTime += std::chrono::seconds(1);
        frameCount = 0;
    }

    return 0;
}

void TimerManager::PrintElapsedTime(void)
{
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - processStartTime);

    int days = elapsed.count() / 86400;        // �Ϸ� = 86400��
    int hours = (elapsed.count() % 86400) / 3600;
    int minutes = (elapsed.count() % 3600) / 60;
    int seconds = elapsed.count() % 60;

    std::wcout << "Elapsed Time: "
        << days << " days  "
        << hours << " hours  "
        << minutes << " minutes  "
        << seconds << " seconds\n";
}