cmake_minimum_required(VERSION 3.25)
project(gaming LANGUAGES CXX)

find_package(absl REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GLEW REQUIRED)
find_package(GTest REQUIRED)

include(FetchContent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPNG REQUIRED IMPORTED_TARGET spng)

FetchContent_Declare(Sodium
    GIT_REPOSITORY https://github.com/robinlinden/libsodium-cmake.git
    GIT_TAG e5b985ad0dd235d8c4307ea3a385b45e76c74c6a # HEAD, last updated at 2025-04-13
)
FetchContent_MakeAvailable(Sodium)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wpedantic -g -Wno-gnu-statement-expression-from-macro-expansion)

add_library(sunset
    src/event_queue.cpp
    src/ecs.cpp
    src/camera.cpp
    src/geometry.cpp
    src/opengl_backend.cpp
    src/rendering.cpp
    src/io_provider.cpp
    src/image.cpp
    src/globals.cpp
    src/backend.cpp
    src/psf2.cpp
    src/physics.cpp
    src/property_tree.cpp
    src/controller.cpp
    src/drm.cpp
    src/crypto.cpp
)

target_include_directories(sunset PUBLIC include)

target_link_libraries(sunset
  PRIVATE
    PkgConfig::SPNG
    absl::base
    absl::log
    absl::log_initialize
    absl::status
    absl::statusor
    glm::glm
    OpenGL::GL
    glfw
    GLEW::GLEW
    ZLIB::ZLIB
)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

add_executable(engine src/main.cpp)
target_link_libraries(engine
  PRIVATE
    absl::base
    absl::log
    absl::log_initialize
    absl::status
    absl::statusor
    glm::glm
    sodium
    sunset)

enable_testing()

add_executable(test_property_tree tests/test_property_tree.cpp)

target_link_libraries(test_property_tree
  PRIVATE
    sunset
    GTest::GTest
    GTest::Main
)

add_test(NAME TestPropertyTree COMMAND test_property_tree)
