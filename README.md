# Kaguya
## Description
This is a hobby project using DirectX 12 and utilizing its latest features such as DirectX RayTracing (DXR). This project have evolved from a path tracer to a deferred renderer. See pathtracing branch for full details of the implementation (Path tracing is still supported in the main branch, just need to change a couple lines of code).

### Showcase

## Path Tracer Showcase
![1](/Gallery/LambertianSpheresInCornellBox.png?raw=true "LambertianSpheresInCornellBox")
![2](/Gallery/GlossySpheresInCornellBox.png?raw=true "GlossySpheresInCornellBox")
![3](/Gallery/TransparentSpheresOfIncreasingIoR.png?raw=true "TransparentSpheresOfIncreasingIoR")

### Features
+ __Path tracing__
+ __Deferred rendering__
+ __Bindless resource__
+ __Multi-threaded commandlist recording__
+ __Lambertian, Glossy, Metal, and Dielectric material models__
+ __Various post processing effects__

### Goals
+ Mesh Shaders
+ Variable Rate Shading
+ Anti-aliasing
+ Denoising via SVGF
+ Incorporate Vulkan

## Build
+ Visual Studio 2019
+ Windows SDK Version 10.0.19041.0
+ CMake Version 3.15

## Source of learning
+ 3D Game Programming with DirectX 12 Book by Frank D Luna
+ Direct3D 12 programming guide from MSDN [https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide]
+ Learning DirectX 12 Tutorial by Jeremiah from [https://www.3dgep.com/learning-directx-12-1/]
+ Physically Based Rendering: From Theory to Implementation by Matt Pharr, Wenzel Jakob, and Greg Humphreys
+ Various GDC talks and slides on different engine architectures (Especially SEED by EA)
+ Various blog posts by incredible people

## Third Party Libraries that are used in the project
+ assimp
+ DirectXTex
+ dxc (DirectX Shader Compiler)
+ imgui
+ spdlog
+ WinPixEventRunTime