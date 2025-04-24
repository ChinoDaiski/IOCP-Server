#pragma once

#include <chrono>

//-----------------------------------------------------------------------------  
// TimerManager.h  
//-----------------------------------------------------------------------------  
#pragma once  
#include <chrono>  
#include <iostream>

class CTimer {
public:
    // ����� Ŭ�� Ÿ��
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    CTimer() noexcept = default;
    ~CTimer() noexcept = default;

    // ��ǥ FPS ���� (Init �� CheckFrame ���)
    void InitTimer(int targetFPS) noexcept;

    // interval ���� �����ϴ� �Լ�
    void ChangeInterval(int targetFPS) noexcept;

    // �� �������� �������� true ��ȯ, �ƴϸ� false
    int CheckFrame() noexcept;

    // ��� �ð��� ����Ͽ� ���
    void PrintElapsedTime();

private:
    int fps = 0;
    TimePoint lastTickTime;
    TimePoint startTime;
    std::chrono::milliseconds interval;
    int frameCount = 0;

    // ó�� ���� ���� �����
    TimePoint processStartTime;
};
