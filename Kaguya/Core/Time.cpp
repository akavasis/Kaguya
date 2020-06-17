#include "pch.h"
#include "Time.h"

Time::Time()
{
	::ZeroMemory(&m_Data, sizeof(m_Data));
	m_Paused = false;

	QueryPerformanceFrequency(&m_Data.Frequency);
	m_Data.Period = 1.0 / static_cast<double>(m_Data.Frequency.QuadPart);
	QueryPerformanceCounter(&m_Data.StartTime);
}

double Time::DeltaTime() const
{
	return m_Data.DeltaTime;
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
		return ((m_Data.StopTime.QuadPart - m_Data.PausedTime.QuadPart) - m_Data.TotalTime.QuadPart) * m_Data.Period;
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
		return ((m_Data.CurrentTime.QuadPart - m_Data.PausedTime.QuadPart) - m_Data.TotalTime.QuadPart) * m_Data.Period;
	}
}

double Time::TotalTimePrecise() const
{
	LARGE_INTEGER CurrentTime;
	QueryPerformanceCounter(&CurrentTime);
	return (CurrentTime.QuadPart - m_Data.StartTime.QuadPart) * m_Data.Period;
}

void Time::Resume()
{
	QueryPerformanceCounter(&m_Data.CurrentTime);
	// Accumulate the time elapsed between stop and start pairs.
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  totalTime       stopTime        startTime     
	if (m_Paused)
	{
		m_Data.PausedTime.QuadPart += (m_Data.CurrentTime.QuadPart - m_Data.StopTime.QuadPart);

		m_Data.PreviousTime = m_Data.CurrentTime;
		m_Data.StopTime.QuadPart = 0;
		m_Paused = false;
	}
}

void Time::Pause()
{
	if (!m_Paused)
	{
		QueryPerformanceCounter(&m_Data.CurrentTime);
		m_Data.StopTime = m_Data.CurrentTime;
		m_Paused = true;
	}
}

void Time::Signal()
{
	if (m_Paused)
	{
		m_Data.DeltaTime = 0.0;
		return;
	}
	QueryPerformanceCounter(&m_Data.CurrentTime);
	// Time difference between this frame and the previous.
	m_Data.DeltaTime = (m_Data.CurrentTime.QuadPart - m_Data.PreviousTime.QuadPart) * m_Data.Period;

	// Prepare for next frame.
	m_Data.PreviousTime = m_Data.CurrentTime;

	// Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then deltaTime can be negative.
	if (m_Data.DeltaTime < 0.0)
	{
		m_Data.DeltaTime = 0.0;
	}
}

void Time::Restart()
{
	QueryPerformanceCounter(&m_Data.CurrentTime);
	m_Data.TotalTime = m_Data.CurrentTime;
	m_Data.PreviousTime = m_Data.CurrentTime;
	m_Data.StopTime.QuadPart = 0;
	m_Paused = false;
	Resume();
}