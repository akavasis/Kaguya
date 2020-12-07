#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <DXProgrammableCapture.h>

namespace Kaguya
{
	namespace Internal
	{
		class PIXCapture
		{
		public:
			PIXCapture();
			~PIXCapture();
		private:
			Microsoft::WRL::ComPtr<IDXGraphicsAnalysis> graphicsAnalysis;
		};
	}
}

#ifdef _DEBUG
#define PIXCapture() Kaguya::Internal::PIXCapture __PIXCAPTURE
#else
#define PIXCapture() 
#endif