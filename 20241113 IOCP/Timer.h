#pragma once

#include <chrono>

class Timer
{
public:
	Timer(void);
	~Timer(void);

public:
    // 경과 시간을 계산하는 함수
	void PrintElapsedTime(void);

private:
	std::chrono::steady_clock::time_point processStartTime;
};

