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
Math/*.cpp
)

file(GLOB_RECURSE External_HeaderList
../External/*.h
)

file(GLOB_RECURSE External_SourceList
../External/*.cpp
)

set(IMGUIZMO_FILE_PATH ${CMAKE_SOURCE_DIR}/Submodules/ImGuizmo)
set(IMGUIZMO 	${IMGUIZMO_FILE_PATH}/ImCurveEdit.cpp
		${IMGUIZMO_FILE_PATH}/ImCurveEdit.h
		${IMGUIZMO_FILE_PATH}/ImGradient.cpp
		${IMGUIZMO_FILE_PATH}/ImGradient.h
		${IMGUIZMO_FILE_PATH}/ImGuizmo.cpp
		${IMGUIZMO_FILE_PATH}/ImGuizmo.h
		${IMGUIZMO_FILE_PATH}/ImSequencer.cpp
		${IMGUIZMO_FILE_PATH}/ImSequencer.h
		${IMGUIZMO_FILE_PATH}/ImZoomSlider.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Main})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PCH_Header})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PCH_Source})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HeaderList})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SourceList})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${External_HeaderList})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${External_SourceList})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${IMGUIZMO})

foreach(Source IN LISTS SourceList)
    set_source_files_properties("${Source}" PROPERTIES COMPILE_FLAGS "/Yupch.h")
endforeach()

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
${IMGUIZMO}
)

# Submodules
include_directories("../Submodules/glm")
include_directories("../Submodules/ImGuizmo")
include_directories("../Submodules/wil/include")

include_directories("../External/assimp/include")
target_link_libraries(${PROJECTNAME} ${CMAKE_SOURCE_DIR}/External/assimp/lib/Release/assimp-vc142-mt.lib)

include_directories("../External/DirectXMesh/include")
target_link_libraries(${PROJECTNAME} debug ${CMAKE_SOURCE_DIR}/External/DirectXMesh/lib/Debug/DirectXMesh.lib)
target_link_libraries(${PROJECTNAME} optimized ${CMAKE_SOURCE_DIR}/External/DirectXMesh/lib/Release/DirectXMesh.lib)

include_directories("../External/DirectXTex/include")
target_link_libraries(${PROJECTNAME} debug ${CMAKE_SOURCE_DIR}/External/DirectXTex/lib/Debug/DirectXTex.lib)
target_link_libraries(${PROJECTNAME} optimized ${CMAKE_SOURCE_DIR}/External/DirectXTex/lib/Release/DirectXTex.lib)

include_directories("../External/DirectXTK12/include")
target_link_libraries(${PROJECTNAME} debug ${CMAKE_SOURCE_DIR}/External/DirectXTK12/lib/Debug/DirectXTK12.lib)
target_link_libraries(${PROJECTNAME} optimized ${CMAKE_SOURCE_DIR}/External/DirectXTK12/lib/Release/DirectXTK12.lib)

include_directories("../External/dxc")
target_link_libraries(${PROJECTNAME} ${CMAKE_SOURCE_DIR}/External/dxc/dxcompiler.lib)

include_directories("../External/imgui")

include_directories("../External/spdlog/include")

include_directories("../External/WinPixEventRuntime/include")
target_link_libraries(${PROJECTNAME} ${CMAKE_SOURCE_DIR}/External/WinPixEventRuntime/bin/x64/WinPixEventRuntime.lib)

add_custom_command(
TARGET ${PROJECTNAME} PRE_BUILD
COMMAND ${CMAKE_COMMAND} -E copy 
	${CMAKE_SOURCE_DIR}/External/assimp/bin/release/assimp-vc142-mt.dll
	${CMAKE_SOURCE_DIR}/External/dxc/dxcompiler.dll
	${CMAKE_SOURCE_DIR}/External/dxc/dxil.dll
	${CMAKE_SOURCE_DIR}/External/WinPixEventRuntime/bin/x64/WinPixEventRuntime.dll
	$<TARGET_FILE_DIR:${PROJECTNAME}>
DEPENDS ${PROJECTNAME})

add_custom_command(
TARGET ${PROJECTNAME} PRE_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/Assets
	$<TARGET_FILE_DIR:${PROJECTNAME}>/Assets
DEPENDS ${PROJECTNAME})

add_custom_command(
TARGET ${PROJECTNAME} PRE_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/Shaders
	$<TARGET_FILE_DIR:${PROJECTNAME}>/Shaders
DEPENDS ${PROJECTNAME})