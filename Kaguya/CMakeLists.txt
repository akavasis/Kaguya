cmake_minimum_required(VERSION 3.15)

set(PROJECTNAME Kaguya)
set(Main "main.cpp")
set(PCH_Header "pch.h")
set(PCH_Source "pch.h" "pch.cpp")
set_source_files_properties("pch.cpp" PROPERTIES COMPILE_FLAGS "/Ycpch.h")

file(GLOB_RECURSE HeaderList
Core/*.h
Graphics/*.h
Math/*.h
Template/*.h

Core/*.inl
Graphics/*.inl
Math/*.inl
Template/*.inl
)

file(GLOB_RECURSE SourceList
Core/*.cpp
Graphics/*.cpp
Input/*.cpp
Math/*.cpp
)

file(GLOB_RECURSE External_HeaderList
../External/*.h
)

file(GLOB_RECURSE External_SourceList
../External/*.cpp
)

file(GLOB_RECURSE Shaders_HeaderList
../Shaders/*.hlsli
)

file(GLOB_RECURSE Shaders_SourceList
../Shaders/*.hlsl
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Main})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PCH_Header})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PCH_Source})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HeaderList})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SourceList})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${External_HeaderList})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${External_SourceList})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${Shaders_HeaderList})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${Shaders_SourceList})

foreach(Source IN LISTS SourceList)
    set_source_files_properties("${Source}" PROPERTIES COMPILE_FLAGS "/Yupch.h")
endforeach()

# Shader properties
set(ShaderModel 6.1)
set_source_files_properties("../Shaders/BlinnPhong_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/BloomComposite_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/BloomMask_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/BRDFIntegration_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/Default_VS.hlsl" PROPERTIES VS_SHADER_TYPE Vertex VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/EquirectangularToCubeMap_CS.hlsl" PROPERTIES VS_SHADER_TYPE Compute VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/GenerateMips_CS.hlsl" PROPERTIES VS_SHADER_TYPE Compute VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/HorizontalBlur_CS.hlsl" PROPERTIES VS_SHADER_TYPE Compute VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/IrradianceConvolution_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/PBR_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/PrefilterConvolution_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/Quad_VS.hlsl" PROPERTIES VS_SHADER_TYPE Vertex VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/Sepia_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/Shadow_VS.hlsl" PROPERTIES VS_SHADER_TYPE Vertex VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/Sky_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/Sky_VS.hlsl" PROPERTIES VS_SHADER_TYPE Vertex VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/Sobel_CS.hlsl" PROPERTIES VS_SHADER_TYPE Compute VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/SobelComposite_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/ToneMapping_PS.hlsl" PROPERTIES VS_SHADER_TYPE Pixel VS_SHADER_MODEL ${ShaderModel})
set_source_files_properties("../Shaders/VerticalBlur_CS.hlsl" PROPERTIES VS_SHADER_TYPE Compute VS_SHADER_MODEL ${ShaderModel})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
include_directories(".")
add_executable(${PROJECTNAME}
${Main}
${PCH_Header}
${PCH_Source}
${HeaderList} 
${SourceList}
${External_HeaderList}
${External_SourceList}
${Shaders_HeaderList}
${Shaders_SourceList}
)

include_directories("../External/assimp/include")
target_link_libraries(${PROJECTNAME} ${CMAKE_SOURCE_DIR}/External/assimp/lib/Release/assimp-vc142-mt.lib)

include_directories("../External/DirectXTex/include")
target_link_libraries(${PROJECTNAME} debug ${CMAKE_SOURCE_DIR}/External/DirectXTex/lib/Debug/DirectXTex.lib)
target_link_libraries(${PROJECTNAME} optimized ${CMAKE_SOURCE_DIR}/External/DirectXTex/lib/Release/DirectXTex.lib)

include_directories("../External/dxc/dxcapi.h")
target_link_libraries(${PROJECTNAME} ${CMAKE_SOURCE_DIR}/External/dxc/dxcompiler.lib)

include_directories("../External/imgui")

include_directories("../External/spdlog/include")

include_directories("../External/WinPixEventRuntime/include")
target_link_libraries(${PROJECTNAME} ${CMAKE_SOURCE_DIR}/External/WinPixEventRuntime/bin/x64/WinPixEventRuntime.lib)