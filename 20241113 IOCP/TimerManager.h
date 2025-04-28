#pragma once

#include <chrono> 

class TimerManager {
public:
    // 사용할 클럭 타입
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    TimerManager() noexcept = default;
    ~TimerManager() noexcept = default;

    // 목표 FPS 설정 (Init 후 CheckFrame 사용)
    void InitTimer(int targetFPS) noexcept;

    // interval 값을 변경하는 함수
    void ChangeInterval(int targetFPS) noexcept;

    // 프레임이 지나기 위해 기다려야하는 시간 반환. 해당 값만큼 Sleep 할 예정
    int CheckFrame() noexcept;

    // 경과 시간을 계산하여 출력
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

    // 처리 시작 시점 저장용
    TimePoint processStartTime;
};
