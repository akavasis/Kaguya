#include "pch.h"
#include "Library.h"

Library::Library
(
	Microsoft::WRL::ComPtr<IDxcBlob> DxcBlob,
	Microsoft::WRL::ComPtr<ID3D12LibraryReflection> LibraryReflection
)
	: m_DxcBlob(DxcBlob)
	, m_LibraryReflection(LibraryReflection)
{

}