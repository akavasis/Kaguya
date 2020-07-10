#pragma once
#include "Mesh.h"
#include "../RenderResourceHandle.h"

struct Skybox
{
	Mesh Mesh;
	RenderResourceHandle VertexBuffer;
	RenderResourceHandle IndexBuffer;
};