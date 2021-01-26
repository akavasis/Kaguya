#pragma once
#include "Component.h"
#include "../../Asset/Mesh.h"
#include "../../Asset/AssetCache.h"

struct MeshFilter : Component
{
	UINT64 MeshID = 0;
	AssetHandle<Asset::Mesh> Mesh = {};
};