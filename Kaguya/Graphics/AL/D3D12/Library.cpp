#include "pch.h"
#include "Library.h"

Library::Library(Microsoft::WRL::ComPtr<IDxcBlob> DxcBlob)
	: m_DxcBlob(DxcBlob)
{
}