project(thirdparty)
add_library(${PROJECT_NAME} INTERFACE)

include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_Declare(
    sdl
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG release-3.2.20
)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp
    GIT_TAG v6.0.2
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb
)


FetchContent_Declare(
        slang
        GIT_REPOSITORY https://github.com/shader-slang/slang
        GIT_TAG v2025.23.1
)


FetchContent_MakeAvailable(glm sdl assimp stb slang)

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} INTERFACE ${Vulkan_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} INTERFACE ${slang_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} INTERFACE
    glm::glm
    SDL3::SDL3
    Vulkan::Vulkan
    assimp
    stb
    slang
)
