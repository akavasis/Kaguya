#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>

struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT2 Texture;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT3 Tangent;

	Vertex() = default;
	Vertex(const DirectX::XMFLOAT3& Position,
		const DirectX::XMFLOAT2& Texture,
		const DirectX::XMFLOAT3& Normal,
		const DirectX::XMFLOAT3& Tangent);
};