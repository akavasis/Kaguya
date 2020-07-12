#pragma once
#include <filesystem>

#include "Mesh.h"

struct Skybox
{
	std::filesystem::path Path;
	Mesh Mesh;
};