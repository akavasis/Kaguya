cmake_minimum_required(VERSION 3.15)

# Group CMakePredefinedTargets into a folder filter
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
project(Kaguya LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED)
add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

add_subdirectory(Kaguya)