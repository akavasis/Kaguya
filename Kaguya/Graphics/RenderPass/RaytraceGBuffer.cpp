#include "pch.h"
#include "RaytraceGBuffer.h"

enum GBufferTypes
{
	WorldPosition,
	WorldNormal,
	MaterialAlbedo,
	MaterialEmissive,
	MaterialSpecular,
	MaterialRefraction,
	MaterialExtra,
	NumGBufferTypes
};