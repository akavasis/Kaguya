#pragma once

#pragma warning(disable : 26812) // prefer enum class over enum warning

// Win32 APIs
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif 

#include <Windows.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <wrl/client.h>

// C++ Standard Libraries
#include <cassert>
#include <memory>
#include <typeinfo>
#include <functional>
#include <algorithm>
#include <numeric>
#include <string>
#include <filesystem>
#include <iostream>
#include <exception>
#include <optional>
#include <variant>
#include <sstream>
#include <codecvt>
#include <optional>
#include <future>

// C++ STL
#include <array>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <queue>

// Operator <=>
#include <compare>

// Submodules
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>

#include <ImGuizmo.h>

#include <entt.hpp>

#include <wil/resource.h>

// External
#include <DirectXTex.h>

#include <nfd.h>
#include <cstdio>
#include <cstdlib>

// DXGI
#include <dxgi1_6.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// D3D12
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")
#include <pix3.h>

#include <Core/Application.h>
#include <Core/Utility.h>
#include <Core/Log.h>
#include <Core/Math.h>
#include <Core/Exception.h>
#include <Core/Synchronization/RWLock.h>
#include <Core/Synchronization/CriticalSection.h>
#include <Core/Synchronization/ConditionVariable.h>

#include <Graphics/Debug/PIXCapture.h>

template<typename T>
void SafeRelease(T*& Ptr)
{
	if (Ptr)
	{
		Ptr->Release();
		Ptr = nullptr;
	}
}