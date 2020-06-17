#include "pch.h"
#include "Vertex.h"

Vertex::Vertex(
	const DirectX::XMFLOAT3& Position,
	const DirectX::XMFLOAT2& Texture,
	const DirectX::XMFLOAT3& Normal,
	const DirectX::XMFLOAT3& Tangent)
	:
	Position(Position),
	Texture(Texture),
	Normal(Normal),
	Tangent(Tangent)
{
}