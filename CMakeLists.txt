cmake_minimum_required(VERSION 3.21)

project(BlackHoleSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(GLEW CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

set(DEPS
    glfw
    GLEW::GLEW
    OpenGL::GL
)

# ===========================
# 2D Black Hole
# ===========================
add_executable(BlackHole2D
    2D_BlackHole.cpp
)

target_link_libraries(BlackHole2D PRIVATE ${DEPS})

# ===========================
# 3D Black Hole
# ===========================
add_executable(BlackHole3D
    3D_BlackHole.cpp
)

target_link_libraries(BlackHole3D PRIVATE ${DEPS})

# ===========================
# Test / Experimental
# ===========================
add_executable(Test
    Test.cpp
)

target_link_libraries(Test PRIVATE ${DEPS})