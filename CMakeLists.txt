cmake_minimum_required(VERSION 3.30)

project(qtrenderer LANGUAGES CXX)

set(CMAKE_PREFIX_PATH "C:/Dev/qt6/qtbase;C:/Users/g/.vcpkg-clion/vcpkg/installed/x64-mingw-dynamic/share")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt6组件
find_package(Qt6 REQUIRED COMPONENTS Core Widgets ShaderTools)

# 公共过滤函数（所有子目录可见）
function(filter_auto_generated_files out_var)
    set(filtered_files ${${out_var}})
    if(filtered_files)
        list(FILTER filtered_files EXCLUDE REGEX ".*moc_.*\\.cpp$")
        list(FILTER filtered_files EXCLUDE REGEX ".*qrc_.*\\.cpp$")
        list(FILTER filtered_files EXCLUDE REGEX ".*/build/.*")
        list(FILTER filtered_files EXCLUDE REGEX ".*CMakeFiles.*")
        list(FILTER filtered_files EXCLUDE REGEX ".*_autogen/.*")
        list(FILTER filtered_files EXCLUDE REGEX ".*\\.dir/.*")
        list(FILTER filtered_files EXCLUDE REGEX "ui_.*\\.h$")
    endif()
    set(${out_var} ${filtered_files} PARENT_SCOPE)
endfunction()

# 添加子目录 - 顺序很重要，被依赖的模块先添加
add_subdirectory(Source/Common)
add_subdirectory(Source/Engine)
add_subdirectory(Source/Editor)
