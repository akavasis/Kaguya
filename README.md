# Kaguya
This is a hobby project using DirectX 12 and utilizing its latest features such as DirectX RayTracing (DXR). This project is a path tracer, more features have yet to be implemented.

# Features
- __Path tracing__
- __Bindless resource__
- __Multi-threaded rendering__
- __Utilization of multiple queues on the GPU__
- __Lambertian, Glossy, Metal, Dielectric material models__
- __ECS scene system (Unity-like interface)__
- __Scene serialization and deserialization using yaml allows quick experimental scene__
- __Asynchronous resource loading__


# Goals
- Implement Spectral path tracing
- Implement multiple anti-aliasing techniques
- Implement complex BRDFs
- Implement importance sampling and multiple importance sampling
- Implement denoising
- Implement compaction to acceleration structures

# Build
- Visual Studio 2019
- Windows SDK Version 10.0.19041.0
- Windows 10 Version: 20H2
- CMake Version 3.15
- C++ 20

Make sure CMake is in your environmental variable path, if not using the CMake Gui should also work. Once you have cloned the repo, be sure
to initialize the submodules.

When the project is build, all the assets and required dlls will be copied to the directory of the executable, there is a Hyperion.yaml in the project folder, if you want to load that in make sure all the file paths for the mesh resource is correct.

# Bibliography
- 3D Game Programming with DirectX 12 Book by Frank D Luna
- Direct3D 12 programming guide from MSDN [https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide]
- Learning DirectX 12 Tutorial by Jeremiah from [https://www.3dgep.com/learning-directx-12-1/]
- Physically Based Rendering: From Theory to Implementation by Matt Pharr, Wenzel Jakob, and Greg Humphreys
- Ray Tracing Gems: High-Quality and Real-Time Rendering with DXR and Other APIs
- Ray Tracing book series (In One Weekend, The Next Week, The Rest of Your Life) by Peter Shirley
- Real-Time Rendering, Fourth Edition by Eric Haines, Naty Hoffman, and Tomas Möller
- Casual Shadertoy Path Tracing series by Alan Wolfe [https://blog.demofox.org/]

# Acknowledgements
- [D3D12MemoryAllocator][1]
- [EnTT][2]
- [imgui][3]
- [ImGuizmo][4]
- [spdlog][5]
- [wil][6]
- [yaml-cpp][7]
- [assimp][8]
- [DirectXMesh][9]
- [DirectXTex][10]
- [DirectXTK12][11]
- [dxc][12]
- [nativefiledialog][13]
- [WinPixEventRuntime][14]

[1]: <https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator> "D3D12MemoryAllocator"
[2]: <https://github.com/skypjack/entt> "EnTT"
[3]: <https://github.com/ocornut/imgui> "imgui"
[4]: <https://github.com/CedricGuillemet/ImGuizmo> "ImGuizmo"
[5]: <https://github.com/gabime/spdlog> "spdlog"
[6]: <https://github.com/microsoft/wil> "wil"
[7]: <https://github.com/jbeder/yaml-cpp> "yaml-cpp"
[8]: <https://github.com/assimp/assimp> "assimp"
[9]: <https://github.com/microsoft/DirectXMesh> "DirectXMesh"
[10]: <https://github.com/microsoft/DirectXTex> "DirectXTex"
[11]: <https://github.com/microsoft/DirectXTK12> "DirectXTK12"
[12]: <https://github.com/microsoft/DirectXShaderCompiler> "dxc"
[13]: <https://github.com/mlabbe/nativefiledialog> "nativefiledialog"
[14]: <https://devblogs.microsoft.com/pix/winpixeventruntime/> "WinPixEventRuntime"

# Showcase
![1](/Gallery/LambertianSpheresInCornellBox.png?raw=true "LambertianSpheresInCornellBox")
![2](/Gallery/GlossySpheresInCornellBox.png?raw=true "GlossySpheresInCornellBox")
![3](/Gallery/TransparentSpheresOfIncreasingIoR.png?raw=true "TransparentSpheresOfIncreasingIoR")
![4](/Gallery/Hyperion.png?raw=true "Hyperion")