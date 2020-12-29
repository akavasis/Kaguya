#include "pch.h"
#include "PIXCapture.h"

#undef PIXCapture

namespace Kaguya
{
	namespace Internal
	{
		PIXCapture::PIXCapture()
		{
			::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&graphicsAnalysis));
			if (graphicsAnalysis)
			{
				graphicsAnalysis->BeginCapture();
			}
		}

		PIXCapture::~PIXCapture()
		{
			if (graphicsAnalysis)
			{
				graphicsAnalysis->EndCapture();
			}
		}
	}
}