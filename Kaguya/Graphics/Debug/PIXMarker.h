#pragma once
#include <d3d12.h>
#include "pix3.h"

namespace Kaguya
{
	namespace Internal
	{
		class PIXMarker
		{
		public:
			PIXMarker(ID3D12GraphicsCommandList* pCommandList, LPCWSTR pMsg);
			~PIXMarker();
		private:
			ID3D12GraphicsCommandList* pCommandList;
		};
	}
}

#ifdef _DEBUG
#define PIXMarker(pCommandList, pMsg) Kaguya::Internal::PIXMarker pixmarker(pCommandList, pMsg)
#else
#define PIXMarker(pCommandList, pMsg) 
#endif