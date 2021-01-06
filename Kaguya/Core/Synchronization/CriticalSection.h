#pragma once

#include <synchapi.h>

class CriticalSection : public CRITICAL_SECTION
{
public:
	CriticalSection(DWORD SpinCount = 0, DWORD Flags = 0)
	{
		::InitializeCriticalSectionEx(this, SpinCount, Flags);
	}

	~CriticalSection()
	{
		::DeleteCriticalSection(this);
	}

	void Enter()
	{
		::EnterCriticalSection(this);
	}

	void Leave()
	{
		::LeaveCriticalSection(this);
	}

	bool TryEnter()
	{
		return ::TryEnterCriticalSection(this);
	}
};

class ScopedCriticalSection
{
public:
	ScopedCriticalSection(CRITICAL_SECTION& CriticalSection)
		: CriticalSection(CriticalSection)
	{
		::EnterCriticalSection(&CriticalSection);
	}

	~ScopedCriticalSection()
	{
		::LeaveCriticalSection(&CriticalSection);
	}

private:
	CRITICAL_SECTION& CriticalSection;
};