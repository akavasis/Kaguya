#pragma once
#include <set>

#include "Scene/Model.h"
#include "Loader/ModelLoader.h"

class ModelAllocator
{
public:
	ModelAllocator();

	Model LoadModel(const char* pPath);
private:
	std::set<ModelData*> m_ModelData;
};