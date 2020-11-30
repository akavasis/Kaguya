#include "pch.h"
#include "PIXEvent.h"

#undef PIXEvent

namespace Kaguya
{
	namespace Internal
	{
		PIXEvent::PIXEvent(ID3D12GraphicsCommandList* pCommandList, LPCWSTR pMsg)
			: pCommandList(pCommandList)
		{
			PIXBeginEvent(pCommandList, 0, pMsg);
		}

		PIXEvent::~PIXEvent()
		{
			PIXEndEvent(pCommandList);
		}
	}
}