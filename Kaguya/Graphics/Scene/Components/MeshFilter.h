#pragma once
#include "Component.h"

struct MeshFilter : Component
{
	struct Mesh* pMesh = nullptr;
};