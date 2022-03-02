cmake_minimum_required(VERSION 3.1.0)

option(MVLG_INCLUDE_VULKAN_HPP ON "include vulkanhpp")
option(MVLG_EXAMPLES ON "build Examples")


set(MVLG_EXTERN "extern")
set(CMAKE_CXX_STANDARD 17)
#option(MVLG_STATIC_LIB ON "build mvlg static lib")

# message("CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
# message("CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_BINARY_DIR}")
# message("CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")
# message("CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")

# if(NOT DEFINED ENV{VK_SDK_PATH})
#     message("Can't find Vulkan SDK Path from Env Variable <VK_SDK_PATH>")
# else()
#     set(MVLG_VK_SDK ENV{VK_SDK_PATH})
#     message(STATUS "$ENV{VK_SDK_PATH}/Include")
# endif()
#message(STATUS "${VK_SDK_PATH}")

project("mvlg")
# MuCplGen Header-Only
include_directories("${MVLG_EXTERN}/MuCplGen/include")

#spv-reflect
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
include_directories("${MVLG_EXTERN}/SPIRV-Reflect")
add_subdirectory("${MVLG_EXTERN}/SPIRV-Reflect")

#vulkan hpp
if(${MVLG_INCLUDE_VULKAN_HPP})
    execute_process(
        WORKING_DIRECTORY "${MVLG_EXTERN}/Vulkan-Hpp"
        COMMAND "git" "submodule update --init Vulkan-Headers"
        OUTPUT_VARIABLE vulkanHeaderPullRes 
    )
    message(STATUS "${vulkanHeaderPullRes}")
    include_directories("${MVLG_EXTERN}/Vulkan-Hpp")
    include_directories("${MVLG_EXTERN}/Vulkan-Hpp/Vulkan-Headers/include")
    message(STATUS "Vulkan-Hpp automatically included")
endif()
#add_subdirectory("${MVLG_EXTERN}/Vulkan-Hpp")

#mvlg header
include_directories("${PROJECT_SOURCE_DIR}/include")

link_directories("$ENV{VK_SDK_PATH}/Lib")
link_libraries("vulkan-1.lib")

set(MVLG_HEADER "${PROJECT_SOURCE_DIR}/include/mvlg/mvlg.h")
set(MVLG_SRC "${PROJECT_SOURCE_DIR}/src/mvlg.cpp")
add_library(mvlg-static
    ${MVLG_HEADER}
    ${MVLG_SRC}
)

if(${MVLG_EXAMPLES})
    message(STATUS "Generate Examples:")
    message(STATUS "\tComputeShader")
    link_libraries(mvlg-static spirv-reflect-static)
    set(computeShaderPath "${PROJECT_SOURCE_DIR}/Examples/ComputeShader")
    add_definitions(-DMU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/")
    add_definitions(-DMU_SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/shader/")
    add_executable(
        ComputeShader
        "${computeShaderPath}/utils.hpp"

        "${computeShaderPath}/FunctionLoader.cpp"
        "${computeShaderPath}/main.cpp"
        "${computeShaderPath}/utils.cpp"
    )
endif()