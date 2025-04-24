cmake_minimum_required(VERSION 3.25)
project(gaming LANGUAGES CXX)

find_package(absl REQUIRED)
find_package(glm REQUIRED)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wpedantic -g)

add_executable(sunset src/main.cpp
    src/event_queue.cpp
    src/ecs.cpp
    src/camera.cpp
    src/geometry.cpp
    # src/io_provider.cpp
    src/rendering.cpp
    src/psf2.cpp
    src/physics.cpp
)

target_include_directories(sunset PRIVATE include)

target_link_libraries(sunset
  PRIVATE
    absl::base
    absl::log
    absl::log_initialize
    absl::status
    absl::statusor
    glm::glm

    OpenGL::GL
    glfw
    GLEW::GLEW
)
