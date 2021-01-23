#pragma once
#include <string>
#include <DirectXTex.h>
#include "../RenderDevice.h"

namespace Asset
{
	struct Image
	{
		std::string Name;
		DirectX::ScratchImage Image;
		bool sRGB = false;

		std::shared_ptr<Resource> Resource;
		Descriptor SRV;
	};
}