#include "pch.h"
#include "PIXCapture.h"

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
				LOG_INFO("PIX Capture begin");
				graphicsAnalysis->BeginCapture();
			}
		}

		PIXCapture::~PIXCapture()
		{
			if (graphicsAnalysis)
			{
				LOG_INFO("PIX Capture end");
				graphicsAnalysis->EndCapture();
			}
		}
	}
}