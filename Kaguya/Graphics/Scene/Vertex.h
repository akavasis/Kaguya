#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>

struct SimpleVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT4 Color;
};

struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT2 Texture;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT3 Tangent;
};