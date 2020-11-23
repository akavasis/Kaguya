#pragma once
#include <Windows.h>

class ConditionVariable : public CONDITION_VARIABLE
{
public:
	ConditionVariable()
	{
		::InitializeConditionVariable(this);
	}
};