#pragma once

#include <chrono> 

class TimerManager {
public:
    // ����� Ŭ�� Ÿ��
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    TimerManager() noexcept = default;
    ~TimerManager() noexcept = default;

    // ��ǥ FPS ���� (Init �� CheckFrame ���)
    void InitTimer(int targetFPS) noexcept;

    // interval ���� �����ϴ� �Լ�
    void ChangeInterval(int targetFPS) noexcept;

    // �������� ������ ���� ��ٷ����ϴ� �ð� ��ȯ. �ش� ����ŭ Sleep �� ����
    int CheckFrame() noexcept;

    // ��� �ð��� ����Ͽ� ���
    void PrintElapsedTime();

    UINT64 GetCurFrame(void) { return totalFrame; }
    TimePoint GetCurrServerTime(void) { return lastTickTime; }


private:
    int fps = 0;
    TimePoint lastTickTime;
    TimePoint startTime;
    std::chrono::milliseconds interval;
    int frameCount = 0;
    UINT64 totalFrame = 0;

    // ó�� ���� ���� �����
    TimePoint processStartTime;
};
