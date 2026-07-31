cmake_minimum_required(VERSION 3.20)

project(opt_mus
    VERSION 0.1.0
    DESCRIPTION "Vulkan tabanli, motorsuz 3D TPS oyunu"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Yapi turu" FORCE)
endif()

# ---------------------------------------------------------------------------
# Bagimliliklar: Vulkan zorunlu; GLFW ve GLM sistemde yoksa indirilir.
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

include(FetchContent)

find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW sistemde bulunamadi, kaynaktan indiriliyor.")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4)
    FetchContent_MakeAvailable(glfw)
endif()

find_package(glm QUIET)
if(NOT glm_FOUND)
    message(STATUS "GLM sistemde bulunamadi, kaynaktan indiriliyor.")
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1)
    FetchContent_MakeAvailable(glm)
endif()

# ---------------------------------------------------------------------------
# Uygulama
# ---------------------------------------------------------------------------
add_executable(opt_mus
    src/main.cpp
    src/core/Application.cpp
    src/core/Window.cpp
    src/scene/Camera.cpp
    src/scene/Player.cpp
    src/render/VulkanCommon.cpp
    src/render/VulkanInstance.cpp
    src/render/VulkanSurface.cpp
    src/render/VulkanDevice.cpp
    src/render/VulkanSwapchain.cpp
    src/render/VulkanPipeline.cpp
    src/render/VulkanBuffer.cpp
    src/render/Mesh.cpp
    src/render/Renderer.cpp
)

target_include_directories(opt_mus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(opt_mus PRIVATE Vulkan::Vulkan glfw glm::glm)

# GLM'in Vulkan kurallarina uymasi icin kritik tanimlar. Ayni tanimlar
# src/core/Math.hpp icinde de yapilir; burasi ikinci guvenlik katmanidir.
target_compile_definitions(opt_mus PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLFW_INCLUDE_VULKAN
    $<$<CONFIG:Debug>:OPTMUS_DEBUG>
)

if(MSVC)
    target_compile_options(opt_mus PRIVATE /W4 /permissive-)
else()
    target_compile_options(opt_mus PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Shader derleme: GLSL -> SPIR-V
# Cikti, calistirilabilir dosyanin yanindaki shaders/ klasorune yazilir.
# ---------------------------------------------------------------------------
find_program(GLSL_COMPILER
    NAMES glslc glslangValidator
    HINTS $ENV{VULKAN_SDK}/bin $ENV{VULKAN_SDK}/Bin
    DOC "GLSL -> SPIR-V derleyicisi")

if(NOT GLSL_COMPILER)
    message(FATAL_ERROR
        "glslc veya glslangValidator bulunamadi. Vulkan SDK kurun ya da "
        "-DGLSL_COMPILER=/yol/glslc ile elle belirtin.")
endif()

set(SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag
)

set(SHADER_OUTPUT_DIR $<TARGET_FILE_DIR:opt_mus>/shaders)
set(SPIRV_BINARIES)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV ${CMAKE_CURRENT_BINARY_DIR}/spv/${SHADER_NAME}.spv)

    if(GLSL_COMPILER MATCHES "glslangValidator")
        set(COMPILE_COMMAND ${GLSL_COMPILER} -V ${SHADER} -o ${SPIRV})
    else()
        set(COMPILE_COMMAND ${GLSL_COMPILER} ${SHADER} -o ${SPIRV})
    endif()

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/spv
        COMMAND ${COMPILE_COMMAND}
        DEPENDS ${SHADER}
        COMMENT "SPIR-V derleniyor: ${SHADER_NAME}"
        VERBATIM)

    list(APPEND SPIRV_BINARIES ${SPIRV})
endforeach()

add_custom_target(shaders DEPENDS ${SPIRV_BINARIES})
add_dependencies(opt_mus shaders)

# SPIR-V dosyalarini calistirilabilir dosyanin yanina kopyala; main.cpp
# shader klasorunu yurutulebilir dosyaya gore cozer.
add_custom_command(TARGET opt_mus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV_BINARIES} ${SHADER_OUTPUT_DIR}
    COMMENT "Shader'lar calistirilabilir dosyanin yanina kopyalaniyor"
    VERBATIM)
