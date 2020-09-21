# Kaguya
How I got the name? Anime. This is my second iteration of DirectX 12 Renderer.

## Description
This is a path tracer using DXR (DirectX Raytracing).  
### Features
+ __Bindless Texturing__
+ __Multi-threaded commandlist recording__
+ __Lambertian, Glossy, Metal, and Dielectric material models__

### Goals
+ Various BSDF models
+ Deferred rendering
+ Global illumination

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
+ tinygltf
+ WinPixEventRunTime