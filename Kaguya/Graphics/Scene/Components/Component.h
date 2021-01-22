#pragma once
#include <entt.hpp>

struct Component
{
	entt::entity Handle = entt::null;
	struct Scene* pScene = nullptr;
	bool IsEdited = false;
};