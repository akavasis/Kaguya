#pragma once
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
			Microsoft::WRL::ComPtr<IDXGraphicsAnalysis> GraphicsAnalysis;
		};
	}
}

#ifdef _DEBUG
#define PIXCapture() Kaguya::Internal::PIXCapture __PIXCAPTURE
#else
#define PIXCapture() 
#endif