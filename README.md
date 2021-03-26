# Kaguya

This is a hobby project using DirectX 12 and utilizing its latest features such as DirectX RayTracing (DXR). This project is inspired by Peter Shirley and his ray tracing book series: [In One Weekend, The Next Week, The Rest of Your Life](https://github.com/RayTracing/raytracing.github.io), Alan Wolfe's blog post series on [causual shadertoy path tracing](https://blog.demofox.org/2020/05/25/casual-shadertoy-path-tracing-1-basic-camera-diffuse-emissive/) and lastly [Physically Based Rendering: From Theory to Implementation](http://www.pbr-book.org/) by Matt Pharr, Wenzel Jakob, and Greg Humphreys.

# Features

- Stochastic path tracing
- Bindless resource
- Multi-threaded rendering
- Utilization of multiple queues on the GPU
- Lambertian, Mirror, Glass, and Disney BSDFs
- Point and quad lights
- ECS scene system (Unity-like interface)
- Scene serialization and deserialization using yaml allows quick experimental scene
- Asynchronous resource loading
- Importance sampling of BSDFs and multiple importance sampling of lights

# Goals

- Implement spectral path tracing
- Implement anti-aliasing techniques
- Implement more materials
- Implement denoising
- Implement compaction to acceleration structures
- Upgrade from DXR 1.0 to DXR 1.1 (inline raytracing)

# Build

- Visual Studio 2019
- GPU that supports DXR
- Windows SDK Version 10.0.19041.0
- Windows 10 Version: 20H2
- CMake Version 3.15
- C++ 20

Make sure CMake is in your environmental variable path, if not using the CMake Gui should also work. Once you have cloned the repo, be sure
to initialize the submodules.

When the project is build, all the assets and required dlls will be copied to the directory of the executable, there's a couple files with the .yaml extension, those can be loaded in when right clicking on hierarchy
and cicking on deserialize.

# Bibliography

- 3D Game Programming with DirectX 12 Book by Frank D Luna
- [Direct3D 12 programming guide from MSDN](https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [Physically Based Rendering: From Theory to Implementation](http://www.pbr-book.org/) by Matt Pharr, Wenzel Jakob, and Greg Humphreys.
- [Ray Tracing Gems: High-Quality and Real-Time Rendering with DXR and Other APIs](http://www.realtimerendering.com/raytracinggems/)
- [Ray Tracing book series (In One Weekend, The Next Week, The Rest of Your Life)](https://github.com/RayTracing/raytracing.github.io) by Peter Shirley
- Real-Time Rendering, Fourth Edition by Eric Haines, Naty Hoffman, and Tomas Möller
- [Casual Shadertoy Path Tracing series](https://blog.demofox.org/) by Alan Wolfe

# Acknowledgements

- [D3D12MemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)
- [EnTT](https://github.com/skypjack/entt)
- [imgui](https://github.com/ocornut/imgui)
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)
- [spdlog](https://github.com/gabime/spdlog)
- [wil](https://github.com/microsoft/wil)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [assimp](https://github.com/assimp/assimp)
- [DirectXTex](https://github.com/microsoft/DirectXTex)
- [DirectXTK12](https://github.com/microsoft/DirectXTK12)
- [dxc](https://github.com/microsoft/DirectXShaderCompiler)
- [nativefiledialog](https://github.com/mlabbe/nativefiledialog)
- [WinPixEventRuntime](https://devblogs.microsoft.com/pix/winpixeventruntime)

# Showcase

![1](/Gallery/LambertianSpheresInCornellBox.png?raw=true "LambertianSpheresInCornellBox")
![2](/Gallery/GlossySpheresInCornellBox.png?raw=true "GlossySpheresInCornellBox")
![3](/Gallery/TransparentSpheresOfIncreasingIoR.png?raw=true "TransparentSpheresOfIncreasingIoR")
![4](/Gallery/Hyperion.png?raw=true "Hyperion")
![5](/Gallery/hyperion_swapchain.png?raw=true "hyperion_swapchain")
![6](/Gallery/hyperion_viewport.png?raw=true "hyperion_viewport")
![7](/Gallery/bedroom_swapchain.png?raw=true "bedroom_swapchain")
![8](/Gallery/bedroom_viewport.png?raw=true "bedroom_viewport")
![9](/Gallery/classroom_swapchain.png?raw=true "classroom_swapchain")
![10](/Gallery/classroom_viewport.png?raw=true "classroom_viewport")
![11](/Gallery/livingroom_swapchain.png?raw=true "livingroom_swapchain")
![12](/Gallery/livingroom_viewport.png?raw=true "livingroom_viewport")
