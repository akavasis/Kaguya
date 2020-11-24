#pragma once
#include <basetsd.h>
#include <Core/Synchronization/RWLock.h>
#include <unordered_map>
#include <vector>

class RenderResourceHandleRegistry
{
public:
	static const UINT64 INVALID_HANDLE_DATA = 1ull << 60ull;

	UINT64 AddNewHandle(const std::string& Name)
	{
		ScopedWriteLock SWL(RWLock);
		auto iter = NameToData.find(Name);
		if (iter != NameToData.end())
			return iter->second;

		DataToName.push_back(Name);
		NameToData[Name] = DataToName.size() - 1;
		return DataToName.size() - 1;
	}

	std::string GetName(UINT64 Index)
	{
		ScopedReadLock SRL(RWLock);
		return DataToName[Index];
	}
private:
	RWLock RWLock;
	std::unordered_map<std::string, UINT64> NameToData;
	std::vector<std::string> DataToName;
};