cmake_minimum_required(VERSION 3.28)

project(Fractal VERSION 1.0 LANGUAGES CXX)

# Set C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch SDL2 dependency
include(FetchContent)
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL
  GIT_TAG release-2.28.5 # Stable SDL2 release
)
FetchContent_MakeAvailable(SDL2)

# Fetch xsimd dependency
FetchContent_Declare(
  xsimd
  GIT_REPOSITORY https://github.com/xtensor-stack/xsimd
  GIT_TAG 13.0.0 # Stable release
)
FetchContent_MakeAvailable(xsimd)

# List of source files
set(SOURCES
  src/main.cpp
  src/Window.cpp
  src/Renderer.cpp
)

# Create the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link against SDL2
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)

# Include directories for project and xsimd
target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${xsimd_SOURCE_DIR}/include
)

# Optional: Enable SIMD optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
  target_compile_options(${PROJECT_NAME} PRIVATE -msse4.2)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
  target_compile_options(${PROJECT_NAME} PRIVATE -march=armv8-a+simd)
endif()