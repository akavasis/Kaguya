#pragma once
// Disable warnings
#pragma warning(disable : 26812) // prefer enum class over enum warning

// Win32 APIs
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#define NOMINMAX
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

// External
#include <DirectXMesh.h>
#include <DirectXTex.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#define GLM_FORCE_XYZW_ONLY
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// D3D12
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")

#include <Core/Application.h>
#include <Core/Utility.h>
#include <Core/Log.h>
#include <Core/Exception.h>
#include <Core/Synchronization/RWLock.h>
#include <Core/Synchronization/CriticalSection.h>
#include <Core/Synchronization/ConditionVariable.h>

#include <Math/MathLibrary.h>

#include <Graphics/Debug/PIXMarker.h>
#include <Graphics/Debug/PIXCapture.h>