#include "pch.h"
#include "PIXCapture.h"
#include <dxgi1_6.h>

#undef PIXCapture

namespace Kaguya
{
	namespace Internal
	{
		PIXCapture::PIXCapture()
		{
			HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&graphicsAnalysis));
			if (graphicsAnalysis)
			{
				CORE_INFO("PIX Capture begin");
				graphicsAnalysis->BeginCapture();
			}
		}

		PIXCapture::~PIXCapture()
		{
			if (graphicsAnalysis)
			{
				CORE_INFO("PIX Capture end");
				graphicsAnalysis->EndCapture();
			}
		}
	}
}