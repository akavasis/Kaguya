#include "pch.h"
#include "PIXCapture.h"

#undef PIXCapture

namespace Kaguya
{
	namespace Internal
	{
		PIXCapture::PIXCapture()
		{
			if (SUCCEEDED(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&GraphicsAnalysis))))
			{
				GraphicsAnalysis->BeginCapture();
			}
		}

		PIXCapture::~PIXCapture()
		{
			if (GraphicsAnalysis)
			{
				GraphicsAnalysis->EndCapture();
			}
		}
	}
}