#pragma once

#include <winnt.h>

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
	LARGE_INTEGER	m_Frequency		= {};
	LARGE_INTEGER	m_StartTime		= {};
	LARGE_INTEGER	m_TotalTime		= {};
	LARGE_INTEGER	m_PausedTime	= {};
	LARGE_INTEGER	m_StopTime		= {};
	LARGE_INTEGER	m_PreviousTime	= {};
	LARGE_INTEGER	m_CurrentTime	= {};
	double			m_Period		= 0.0; // Represents the amount of time it takes for 1 tick/oscilation
	double			m_DeltaTime		= 0.0;

	bool			m_Paused		= false;
};