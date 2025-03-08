find_package(Vulkan REQUIRED)

add_subdirectory(glm)
add_subdirectory(sdl)
add_subdirectory(assimp)

include_directories(${Vulkan_INCLUDE_DIR} SYSTEM)
include_directories(${GLM_INCLUDE_DIR} SYSTEM)
include_directories(${SDL3_INCLUDE_DIR} SYSTEM)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)