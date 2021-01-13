# Kaguya
This is a hobby project using DirectX 12 and utilizing its latest features such as DirectX RayTracing (DXR). This project have evolved from a path tracer to a deferred renderer. See pathtracing branch for full details of the implementation (Path tracing is still supported in the main branch, just need to change a couple lines of code).

# Features
- __Path tracing__
- __Deferred rendering__
- __Hybrid renderer (Utilize both rasterization and ray tracing)__
- __Bindless resource__
- __Multi-threaded commandlist recording__
- __Utilization of multiple queues on the GPU__
- __Lambertian, Glossy, Metal, Dielectric and standard PBR material models__
- __Post Processing on compute shaders__
- __Rectangular area light via linearly transformed cosines (LTC)__
- __Ray-traced soft shadows__
- __Shadow denoising via SVGF__
- __Mesh shader__

# Goals
- Experiment with DirectX 12 Ultimate features: DirectX Raytracing 1.1, Variable Rate Shading, Mesh Shaders, and Sampler Feedback
- Implement multiple anti-aliasing techniques
- Resume reading ray tracing books to implement a fully functional path tracer while maintaining the deferred renderer
- Implement memory aliasing between render passes in the render graph
- Implement compaction to acceleration structures

# Build
- Visual Studio 2019
- Windows SDK Version 10.0.19041.0
- Windows 10 Version: 20H2
- CMake Version 3.15

Make sure CMake is in your environmental variable path, if not using the CMake Gui should also work. Once you have cloned the repo, be sure
to initialize the submodules.

# Bibliography
- 3D Game Programming with DirectX 12 Book by Frank D Luna
- Direct3D 12 programming guide from MSDN [https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide]
- Learning DirectX 12 Tutorial by Jeremiah from [https://www.3dgep.com/learning-directx-12-1/]
- Physically Based Rendering: From Theory to Implementation by Matt Pharr, Wenzel Jakob, and Greg Humphreys
- Ray Tracing Gems: High-Quality and Real-Time Rendering with DXR and Other APIs
- Ray Tracing book series (In One Weekend, The Next Week, The Rest of Your Life) by Peter Shirley
- Real-Time Rendering, Fourth Edition by Eric Haines, Naty Hoffman, and Tomas Möller

# Acknowledgements
- [assimp][1]
- [DirectXMesh][2]
- [DirectXTex][3]
- [DirectXTK12][4]
- [DirectX Shader Compiler][5]
- [imgui][6]
- [ImGuizmo][7]
- [spdlog][8]
- [WinPixEventRunTime][9]
- [wil][10]

[1]: <https://github.com/assimp/assimp> "assimp"
[2]: <https://github.com/microsoft/DirectXMesh> "DirectXMesh"
[3]: <https://github.com/microsoft/DirectXTex> "DirectXTex"
[4]: <https://github.com/microsoft/DirectXTK12> "DirectXTK12"
[5]: <https://github.com/microsoft/DirectXShaderCompiler> "DirectX Shader Compiler"
[6]: <https://github.com/ocornut/imgui> "imgui"
[7]: <https://github.com/CedricGuillemet/ImGuizmo> "ImGuizmo"
[8]: <https://github.com/gabime/spdlog> "spdlog"
[9]: <https://devblogs.microsoft.com/pix/winpixeventruntime/> "WinPixEventRunTime"
[10]: <https://github.com/microsoft/wil> "wil"

# Showcase

# Deferred Renderer Showcase
![1](/Gallery/DeferredRenderer_CornellBox_Keyblade.png?raw=true "DeferredRenderer_CornellBox_Keyblade")

# Path Tracer Showcase
![1](/Gallery/LambertianSpheresInCornellBox.png?raw=true "LambertianSpheresInCornellBox")
![2](/Gallery/GlossySpheresInCornellBox.png?raw=true "GlossySpheresInCornellBox")
![3](/Gallery/TransparentSpheresOfIncreasingIoR.png?raw=true "TransparentSpheresOfIncreasingIoR")
![4](/Gallery/Hyperion.jpg?raw=true "Hyperion")
![5](/Gallery/HyperionWithEgg.jpg?raw=true "HyperionWithEgg")

## Picking/Editing Progress
![1](/Gallery/PickingAndEditing.jpg?raw=true "PickingAndEditing")