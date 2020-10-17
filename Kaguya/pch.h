#pragma once
// Disable warnings
#pragma warning(disable : 26812) // prefer enum class over enum warning

// Win32 APIs
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <wrl/client.h>

// C++ Standard Libraries
#include <assert.h>
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

// C++ STL
#include <array>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <queue>

// operator <=>
#include <compare>

// third parties
#include <DirectXTex.h>
#include "../External/DirectXTex/D3D12/d3dx12.h"
#include "../External/imgui/imgui.h"
#include "../External/imgui/imgui_impl_win32.h"
#include "../External/imgui/imgui_impl_dx12.h"

// DX12
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")

#include <Core/Utility.h>
#include <Core/Log.h>
#include <Core/Exception.h>
#include <Math/MathLibrary.h>
#include <Graphics/Debug/PIXMarker.h>
#include <Graphics/Debug/PIXCapture.h>