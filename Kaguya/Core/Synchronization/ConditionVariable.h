#pragma once

#include <synchapi.h>

class ConditionVariable : public CONDITION_VARIABLE
{
public:
	ConditionVariable()
	{
		::InitializeConditionVariable(this);
	}
};