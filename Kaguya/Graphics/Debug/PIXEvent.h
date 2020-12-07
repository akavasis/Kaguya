#pragma once
#include <d3d12.h>
#include "pix3.h"

namespace Kaguya
{
	namespace Internal
	{
		class PIXEvent
		{
		public:
			PIXEvent(ID3D12GraphicsCommandList* pCommandList, LPCWSTR pMsg);
			~PIXEvent();
		private:
			ID3D12GraphicsCommandList* pCommandList;
		};
	}
}

#ifdef _DEBUG
#define PIXEvent(pCommandList, pMsg) Kaguya::Internal::PIXEvent __PIXEVENT(pCommandList, pMsg)
#else
#define PIXEvent(pCommandList, pMsg) 
#endif