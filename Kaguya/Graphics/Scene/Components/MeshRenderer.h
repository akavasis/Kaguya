#pragma once
#include "Component.h"
#include "Private/Material.h"

struct MeshRenderer : Component
{
	struct MeshFilter* pMeshFilter = nullptr;
	Material Material;
};