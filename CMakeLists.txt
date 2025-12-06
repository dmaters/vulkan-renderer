cmake_minimum_required(VERSION 3.7)
project(Renderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE LOCAL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${LOCAL_SOURCES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

target_include_directories(${PROJECT_NAME} PRIVATE src/)
target_compile_definitions(${PROJECT_NAME} PRIVATE VULKAN_HPP_NO_CONSTRUCTORS)
target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)

add_subdirectory(thirdparty)
target_link_libraries(${PROJECT_NAME} PRIVATE thirdparty)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /DEBUG:FULL /EHsc /W4)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif ()


if (WIN32)
    get_target_property(DLL_PATHS slang INTERFACE_DLL_PATH)
    foreach (DLL ${DLL_PATHS})
        add_custom_command(
                TARGET ${PROJECT_NAME}
                POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}" ${CMAKE_BINARY_DIR}
        )
    endforeach ()
endif ()