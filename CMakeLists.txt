cmake_minimum_required(VERSION 3.28)

project(Fractal VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/arch:AVX2)
else()
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    add_compile_options(-msse4.2)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
    add_compile_options(-march=armv8-a+simd)
  endif()
endif()

include(FetchContent)

FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL
  GIT_TAG release-2.28.5
)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(
  xsimd
  GIT_REPOSITORY https://github.com/xtensor-stack/xsimd
  GIT_TAG 13.0.0
)
FetchContent_MakeAvailable(xsimd)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(FractalCore
  src/Window.cpp
  src/Renderer.cpp
)

target_include_directories(FractalCore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${xsimd_SOURCE_DIR}/include"
)

target_link_libraries(FractalCore PUBLIC SDL2::SDL2 SDL2::SDL2main)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE FractalCore)

if(WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:SDL2::SDL2>
  $<TARGET_FILE_DIR:${PROJECT_NAME}>
  )
endif()

enable_testing()

add_executable(FractalTests tests/test_main.cpp)

target_link_libraries(FractalTests PRIVATE 
  FractalCore 
  GTest::gtest_main 
  xsimd
)

target_include_directories(FractalTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(WIN32)
  add_custom_command(TARGET FractalTests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  $<TARGET_FILE:SDL2::SDL2>
  $<TARGET_FILE_DIR:FractalTests>
  )
endif()

include(GoogleTest)
gtest_discover_tests(FractalTests)
