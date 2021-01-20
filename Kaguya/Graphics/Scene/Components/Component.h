#pragma once
#include <entt.hpp>

struct Component
{
	entt::entity Handle;
	struct Scene* pScene;
};