cmake_minimum_required(VERSION 3.24)
project(hrdesk CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(HRDESK_VERSION "0.0.1")
add_definitions(-DHRDESK_VERSION="${HRDESK_VERSION}")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -march=native -DNDEBUG")

find_package(OpenGL REQUIRED)

include(FetchContent)
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG docking GIT_SHALLOW TRUE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG 3.4 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui glfw)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(hrdesk src/main.cpp src/app/Fonts.cpp)
target_link_libraries(hrdesk PRIVATE imgui)

# Копируем assets в каталог сборки и указываем путь к ним приложению
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets)
add_dependencies(hrdesk copy_assets)
target_include_directories(hrdesk PRIVATE ${imgui_SOURCE_DIR})
add_definitions(-DHRDESK_ASSETS_DIR="${CMAKE_BINARY_DIR}/assets")
