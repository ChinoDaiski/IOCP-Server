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
    // 사용할 클럭 타입
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    CTimer() noexcept = default;
    ~CTimer() noexcept = default;

    // 목표 FPS 설정 (Init 후 CheckFrame 사용)
    void InitTimer(int targetFPS) noexcept;

    // interval 값을 변경하는 함수
    void ChangeInterval(int targetFPS) noexcept;

    // 한 프레임이 지났으면 true 반환, 아니면 false
    int CheckFrame() noexcept;

    // 경과 시간을 계산하여 출력
    void PrintElapsedTime();

private:
    int fps = 0;
    TimePoint lastTickTime;
    TimePoint startTime;
    std::chrono::milliseconds interval;
    int frameCount = 0;

    // 처리 시작 시점 저장용
    TimePoint processStartTime;
};
