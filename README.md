# Kaguya
## Description
This is a hobby project using DirectX 12 and utilizing its latest features such as DirectX RayTracing (DXR). This project have evolved from a path tracer to a deferred renderer. See pathtracing branch for full details of the implementation (Path tracing is still supported in the main branch, just need to change a couple lines of code).

# Features
- __Path tracing__
- __Deferred rendering__
- __Bindless resource__
- __Multi-threaded commandlist recording__
- __Utilization of multiple queues on the GPU__
- __Lambertian, Glossy, Metal, Dielectric and standard PBR material models__
- __Post Processing on compute shaders__
- __Rectangular area light via linearly transformed cosines (LTC)__
- __Ray-traced soft shadows__
- __Shadow denoising via SVGF__

# Goals
- Experiment with DirectX 12 Ultimate features: DirectX Raytracing 1.1, Variable Rate Shading, Mesh Shaders, and Sampler Feedback
- Implement multiple anti-aliasing techniques
- Resume reading ray tracing books to implement a fully functional path tracer while maintaining the deferred renderer

# Build
- Visual Studio 2019
- Windows SDK Version 10.0.19041.0
- CMake Version 3.15

# Bibliography
- 3D Game Programming with DirectX 12 Book by Frank D Luna
- Direct3D 12 programming guide from MSDN [https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide]
- Learning DirectX 12 Tutorial by Jeremiah from [https://www.3dgep.com/learning-directx-12-1/]
- Physically Based Rendering: From Theory to Implementation by Matt Pharr, Wenzel Jakob, and Greg Humphreys
- Ray Tracing Gems: High-Quality and Real-Time Rendering with DXR and Other APIs
- Ray Tracing book series (In One Weekend, The Next Week, The Rest of Your Life) by Peter Shirley
- Real-Time Rendering, Fourth Edition by Eric Haines, Naty Hoffman, and Tomas Möller

# Acknowledgements
- assimp (https://github.com/assimp/assimp)
- DirectXTex (https://github.com/microsoft/DirectXTex)
- DirectX Shader Compiler (https://github.com/microsoft/DirectXShaderCompiler)
- imgui (https://github.com/ocornut/imgui)
- spdlog (https://github.com/gabime/spdlog)
- WinPixEventRunTime (https://devblogs.microsoft.com/pix/winpixeventruntime/)

# Showcase

# Deferred Renderer Showcase
![1](/Gallery/DeferredRenderer_CornellBox_Keyblade.png?raw=true "DeferredRenderer_CornellBox_Keyblade")

# Path Tracer Showcase
![1](/Gallery/LambertianSpheresInCornellBox.png?raw=true "LambertianSpheresInCornellBox")
![2](/Gallery/GlossySpheresInCornellBox.png?raw=true "GlossySpheresInCornellBox")
![3](/Gallery/TransparentSpheresOfIncreasingIoR.png?raw=true "TransparentSpheresOfIncreasingIoR")