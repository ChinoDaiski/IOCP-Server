#pragma once

#include <chrono>

class Timer
{
public:
	Timer(void);
	~Timer(void);

public:
    // ��� �ð��� ����ϴ� �Լ�
	void PrintElapsedTime(void);

private:
	std::chrono::steady_clock::time_point processStartTime;
};

