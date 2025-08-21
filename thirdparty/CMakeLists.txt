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

set(OPTION_ENABLE_ALL_APPS OFF)
set(OPTION_BUILD_KTX2 OFF)
set(OPTION_BUILD_EXR OFF)
set(OPTION_BUILD_GUI OFF)
set(OPTION_BUILD_BROTLIG OFF)
set(OPTION_BUILD_BROTLIG_GUI OFF)
set(OPTION_BUILD_CMP_SDK ON)

FetchContent_Declare(
    compressonator
    GIT_REPOSITORY https://github.com/GPUOpen-Tools/compressonator
    GIT_TAG V4.5.52
)

FetchContent_MakeAvailable(glm sdl assimp stb compressonator)

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} INTERFACE ${Vulkan_INCLUDE_DIR})

if(WIN32)
    configure_file(
        "$ENV{VULKAN_SDK}/bin/slang-glslang.dll"
        "${CMAKE_BINARY_DIR}/slang-glslang.dll"
        COPYONLY
    )
endif()

target_link_directories(${PROJECT_NAME} INTERFACE "$ENV{VULKAN_SDK}/lib")

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} INTERFACE
    glm::glm
    SDL3::SDL3
    Vulkan::Vulkan
    assimp
    stb
    slang
    CMP_Compressonator
)
