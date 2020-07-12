#include "pch.h"
#include "PIXMarker.h"

#undef PIXMarker

namespace Kaguya
{
	namespace Internal
	{
		PIXMarker::PIXMarker(ID3D12GraphicsCommandList* pCommandList, LPCWSTR pMsg)
			: pCommandList(pCommandList)
		{
			PIXBeginEvent(pCommandList, 0, pMsg);
		}

		PIXMarker::~PIXMarker()
		{
			PIXEndEvent(pCommandList);
		}
	}
}