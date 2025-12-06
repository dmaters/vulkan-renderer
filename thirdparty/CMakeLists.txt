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

FetchContent_MakeAvailable(glm sdl assimp stb)


set(SLANG_VERSION "2025.23.2")

if (WIN32)
    set(SLANG_OS "windows-x86_64")
elseif (UNIX)
    set(SLANG_OS "linux-x86_64")
endif ()

include(FetchContent)
FetchContent_Populate(slang_zip URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_OS}.zip QUIET)

add_library(slang INTERFACE)

target_include_directories(slang INTERFACE "${slang_zip_SOURCE_DIR}/include")
if (WIN32)
    target_link_libraries(slang INTERFACE "${slang_zip_SOURCE_DIR}/lib/slang-compiler.lib")

    set_target_properties(slang PROPERTIES
            INTERFACE_DLL_PATH "${slang_zip_SOURCE_DIR}/bin/slang-compiler.dll;${slang_zip_SOURCE_DIR}/bin/slang-glslang.dll"
    )
elseif (UNIX)
    target_link_libraries(slang INTERFACE "${slang_zip_SOURCE_DIR}/lib/libslang-compiler.so.0.${SLANG_VERSION}"
    )
endif ()


find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} INTERFACE ${Vulkan_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} INTERFACE ${stb_SOURCE_DIR})


target_link_libraries(${PROJECT_NAME} INTERFACE
        glm::glm
        SDL3::SDL3
        Vulkan::Vulkan
        assimp
        slang
)
