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
    texture-compressor
    GIT_REPOSITORY https://github.com/dmaters/texture-compressor
)
FetchContent_MakeAvailable(glm sdl assimp stb texture-compressor)

include(slang.cmake)
include(imgui.cmake)


find_package(Vulkan REQUIRED)
target_link_libraries(imgui INTERFACE SDL3::SDL3 Vulkan::Vulkan)

target_include_directories(${PROJECT_NAME} INTERFACE ${Vulkan_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} INTERFACE ${stb_SOURCE_DIR})

target_compile_options(assimp INTERFACE -Wno-error)
target_link_libraries(
    ${PROJECT_NAME} INTERFACE
    glm::glm
    SDL3::SDL3
    Vulkan::Vulkan
    assimp
    slang
    imgui
    texture-compressor
)
