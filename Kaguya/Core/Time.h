#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class Time
{
public:
	Time();

	double DeltaTime() const;
	double TotalTime() const;
	double TotalTimePrecise() const;

	void Resume();
	void Pause();
	void Signal();
	void Restart();
private:
	struct Data
	{
		LARGE_INTEGER Frequency;
		LARGE_INTEGER StartTime;
		LARGE_INTEGER TotalTime;
		LARGE_INTEGER PausedTime;
		LARGE_INTEGER StopTime;
		LARGE_INTEGER PreviousTime;
		LARGE_INTEGER CurrentTime;
		double Period; // Represents the amount of time it takes for 1 tick/oscilation
		double DeltaTime;
	} m_Data;

	bool m_Paused;
};