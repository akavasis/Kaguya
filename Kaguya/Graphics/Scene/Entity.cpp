#include "pch.h"
#include "Entity.h"

Entity::Entity(entt::entity Handle, Scene* pScene)
	: Handle(Handle)
	, pScene(pScene)
{

}