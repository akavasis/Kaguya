#include "pch.h"
#include "Time.h"

Time::Time()
{
	::QueryPerformanceFrequency(&m_Frequency);
	m_Period = 1.0 / static_cast<double>(m_Frequency.QuadPart);
	::QueryPerformanceCounter(&m_StartTime);
}

double Time::DeltaTime() const
{
	return m_DeltaTime;
}

double Time::TotalTime() const
{
	// If we are stopped, do not count the time that has passed since we stopped.
	// Moreover, if we previously already had a pause, the distance 
	// stopTime - totalTime includes paused time, which we do not want to count.
	// To correct this, we can subtract the paused time from stopTime:  
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  totalTime       stopTime        startTime     stopTime    currentTime
	if (m_Paused)
	{
		return ((m_StopTime.QuadPart - m_PausedTime.QuadPart) - m_TotalTime.QuadPart) * m_Period;
	}
	// The distance currentTime - totalTime includes paused time,
	// which we do not want to count.  
	// To correct this, we can subtract the paused time from currentTime:  
	// (currentTime - pausedTime) - totalTime 
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------> time
	//  totalTime       stopTime        startTime     currentTime
	else
	{
		return ((m_CurrentTime.QuadPart - m_PausedTime.QuadPart) - m_TotalTime.QuadPart) * m_Period;
	}
}

double Time::TotalTimePrecise() const
{
	LARGE_INTEGER CurrentTime;
	::QueryPerformanceCounter(&CurrentTime);
	return (CurrentTime.QuadPart - m_StartTime.QuadPart) * m_Period;
}

void Time::Resume()
{
	::QueryPerformanceCounter(&m_CurrentTime);
	// Accumulate the time elapsed between stop and start pairs.
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  totalTime       stopTime        startTime     
	if (m_Paused)
	{
		m_PausedTime.QuadPart += (m_CurrentTime.QuadPart - m_StopTime.QuadPart);

		m_PreviousTime = m_CurrentTime;
		m_StopTime.QuadPart = 0;
		m_Paused = false;
	}
}

void Time::Pause()
{
	if (!m_Paused)
	{
		::QueryPerformanceCounter(&m_CurrentTime);
		m_StopTime = m_CurrentTime;
		m_Paused = true;
	}
}

void Time::Signal()
{
	if (m_Paused)
	{
		m_DeltaTime = 0.0;
		return;
	}
	::QueryPerformanceCounter(&m_CurrentTime);
	// Time difference between this frame and the previous.
	m_DeltaTime = (m_CurrentTime.QuadPart - m_PreviousTime.QuadPart) * m_Period;

	// Prepare for next frame.
	m_PreviousTime = m_CurrentTime;

	// Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then deltaTime can be negative.
	if (m_DeltaTime < 0.0)
	{
		m_DeltaTime = 0.0;
	}
}

void Time::Restart()
{
	::QueryPerformanceCounter(&m_CurrentTime);
	m_TotalTime = m_CurrentTime;
	m_PreviousTime = m_CurrentTime;
	m_StopTime.QuadPart = 0;
	m_Paused = false;
	Resume();
}