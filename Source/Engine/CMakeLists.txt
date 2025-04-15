cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(Engine STATIC)

# 查找源文件
file(GLOB_RECURSE ENGINE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE ENGINE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

# 过滤自动生成的文件
filter_auto_generated_files(ENGINE_SOURCES)
filter_auto_generated_files(ENGINE_HEADERS)

# 添加源文件
target_sources(Engine PRIVATE
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

# 添加依赖
target_link_libraries(Engine PRIVATE
    Common
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
)

target_include_directories(Engine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
)

target_include_directories(Engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

# 设置静态库输出目录
set_target_properties(Engine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
