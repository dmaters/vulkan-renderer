project(thirdparty)
add_library(${PROJECT_NAME} INTERFACE)


include(FetchContent)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.5.1
)

FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG v0.9.0
)

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb
)

FetchContent_Declare(
    texture-compressor
    GIT_REPOSITORY https://github.com/dmaters/texture-compressor
)
FetchContent_MakeAvailable(glm glfw fastgltf stb texture-compressor)

include(slang.cmake)
include(imgui.cmake)


find_package(Vulkan REQUIRED)
target_link_libraries(imgui INTERFACE glfw Vulkan::Vulkan)

target_include_directories(${PROJECT_NAME} INTERFACE ${Vulkan_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} INTERFACE ${stb_SOURCE_DIR})

target_link_libraries(
    ${PROJECT_NAME} INTERFACE
    glm::glm
    glfw
    Vulkan::Vulkan
    fastgltf
    slang
    imgui
    texture-compressor
)
