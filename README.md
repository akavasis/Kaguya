# KHDirectX
Personal project exploring graphics using DirectX 12

## Description
This is a personal project exploring the DirectX 12 API. Right now the renderer is able to do:
* Various post processing effects
* Load in various model files utilizing Assimp
* Frustum culling on CPU
* HDR Rendering, tone mapping
* Directional cascade shadow mapping
* Normal mapping, Cube mapping
* PBR Pipeline
* Expanded graphics pipeline into a render graph system
* More to come: SSAO, Deferred rendering, more post process effects,  
asynchronous loading of models/materials/textures/environment maps,  
dynamically control the scene via a editor UI, Multi-threaded job system

## Source of learning
1. 3D Game Programming with DirectX 12 Book by Frank D Luna
2. Direct3D 12 programming guide from MSDN [https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide]
3. Learning DirectX 12 Tutorial by Jeremiah from [https://www.3dgep.com/learning-directx-12-1/]

## Third Party Libraries that are used in the project
1. Assimp
2. DirectXTex
3. ImGui
4. spdlog
5. WinPixEventRunTime

## Screenshots
![csm](/Screenshot/csm.png?raw=true "CSM")
![csmdebug](/Screenshot/csmdebug.png?raw=true "CSM Debug")
![pbr](/Screenshot/pbr.png?raw=true "PBR")
![pbr2](/Screenshot/pbr2.png?raw=true "PBR2")
![rgdebug](/Screenshot/rgdebug.png?raw=true "Render Graph Debug")