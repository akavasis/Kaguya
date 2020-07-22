# Kaguya
How I got the name? Anime. This is my second iteration of DirectX 12 Renderer.

## Description
The main feature of this new iteration is __Bindless Texturing__ using SM6+ feature: unbound arrays and __Multi-threaded__ commandlist recording via fork-join approach using the __Render Graph__ system. Some more features I would like to add is memory aliasing using Heaps based on D3D12_RESOURCE_HEAP_TIER which then allows memory reuse between different passes. Some of the features from my first iteration (DX12) are carried over such as cascaded shadow mapping, post processing, physically based rendering.

## Goals
1. Add DXR (DirectX Raytracing)
2. Add Deferred Rendering
3. Add Volumetric Lighting
4. Add Global Illumination
5. Explore new DirectX 12 Ultimate features such as Mesh Shaders and Variable Rate Shading

## Source of learning
1. 3D Game Programming with DirectX 12 Book by Frank D Luna
2. Direct3D 12 programming guide from MSDN [https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide]
3. Learning DirectX 12 Tutorial by Jeremiah from [https://www.3dgep.com/learning-directx-12-1/]
4. Various GDC talks and slides on different engine architectures (Especially SEED by EA)

## Third Party Libraries that are used in the project
1. Assimp
2. DirectXTex
3. DXC (DirectX Shader Compiler)
3. ImGui
4. spdlog
5. WinPixEventRunTime