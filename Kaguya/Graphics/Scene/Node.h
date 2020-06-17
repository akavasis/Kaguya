#pragma once
#include <memory>
#include <string>
#include "Math/Transform.h"
#include "Mesh.h"

struct Node
{
	Node* pParent = nullptr;
	INT ID;
	std::string Name;
	Transform Transform;
	DirectX::BoundingBox BoundingBox;
	std::vector<Mesh*> pMeshes;
	std::vector<std::unique_ptr<Node>> pChildren;

	Node();
	~Node();

	void AddChild(std::unique_ptr<Node> pChild);

	void SetTransform(DirectX::FXMMATRIX M);
	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void Rotate(float AngleX, float AngleY, float AngleZ);
};