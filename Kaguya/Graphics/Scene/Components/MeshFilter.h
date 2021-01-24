#pragma once
#include "Component.h"

namespace Asset
{
	struct Mesh;
}

struct MeshFilter : Component
{
	Asset::Mesh* pMesh = nullptr;
};