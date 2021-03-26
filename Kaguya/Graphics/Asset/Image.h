#pragma once
#include <string>
#include <DirectXTex.h>
#include "../RenderDevice.h"

namespace Asset
{
	struct Image;

	struct ImageMetadata
	{
		std::filesystem::path Path;
		bool sRGB;
	};

	struct Image
	{
		ImageMetadata Metadata;

		std::string Name;
		DirectX::ScratchImage Image;

		std::shared_ptr<Resource> Resource;
		Descriptor SRV;
	};
}