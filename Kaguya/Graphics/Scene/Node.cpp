#include "pch.h"
#include "Node.h"

Node::Node()
{
	SetTransform(DirectX::XMMatrixIdentity());
}

Node::~Node()
{
}

void Node::AddChild(std::unique_ptr<Node> pChild)
{
	pChild->pParent = this;
	pChildren.push_back(std::move(pChild));
}

void Node::SetTransform(DirectX::FXMMATRIX M)
{
	Transform.SetTransform(M);

	size_t i, size;
	for (i = 0, size = pMeshes.size(); i < size; ++i)
	{
		pMeshes[i]->Transform.SetTransform(M);
	}
	for (i = 0, size = pChildren.size(); i < size; ++i)
	{
		pChildren[i]->SetTransform(M);
	}
}

void Node::Translate(float DeltaX, float DeltaY, float DeltaZ)
{
	Transform.Translate(DeltaX, DeltaY, DeltaZ);

	size_t i, size;
	for (i = 0, size = pMeshes.size(); i < size; ++i)
	{
		pMeshes[i]->Transform.Translate(DeltaX, DeltaY, DeltaZ);
	}
	for (i = 0, size = pChildren.size(); i < size; ++i)
	{
		pChildren[i]->Translate(DeltaX, DeltaY, DeltaZ);
	}
}

void Node::Rotate(float AngleX, float AngleY, float AngleZ)
{
	Transform.Rotate(AngleX, AngleY, AngleZ);

	size_t i, size;
	for (i = 0, size = pMeshes.size(); i < size; ++i)
	{
		pMeshes[i]->Transform.Rotate(AngleX, AngleY, AngleZ);
	}
	for (i = 0, size = pChildren.size(); i < size; ++i)
	{
		pChildren[i]->Rotate(AngleX, AngleY, AngleZ);
	}
}