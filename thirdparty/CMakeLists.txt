find_package(Vulkan REQUIRED)

include_directories(${Vulkan_INCLUDE_DIR} SYSTEM)
add_subdirectory(glm)
add_subdirectory(sdl)
add_subdirectory(assimp)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)
