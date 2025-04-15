cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui ShaderTools)

file(GLOB_RECURSE EDITOR_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/Public/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp
)
file(GLOB_RECURSE EDITOR_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.h
)

filter_auto_generated_files(EDITOR_SOURCES)
filter_auto_generated_files(EDITOR_HEADERS)

add_executable(Editor ${EDITOR_SOURCES} ${EDITOR_HEADERS})

set_target_properties(Editor PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC ON
)

# 添加Qt资源文件
set(EDITOR_RESOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/img.qrc
        # resources.qrc
        # 添加其他资源文件...
)

qt_add_resources(EDITOR_RESOURCES_SOURCES ${EDITOR_RESOURCES})
target_sources(Editor PRIVATE ${EDITOR_RESOURCES_SOURCES})

# 添加shader编译功能
if (TARGET Qt6::ShaderTools)
    message(STATUS "ShaderTools target exists")

    find_program(QSB_EXECUTABLE qsb REQUIRED)
    message(STATUS "Found qsb at: ${QSB_EXECUTABLE}")

    file(GLOB_RECURSE SHADER_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.frag
            ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.vert
    )

    set(COMPILED_SHADERS "")
    foreach (SHADER_FILE ${SHADER_FILES})
        file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Shaders ${SHADER_FILE})
        string(REPLACE "\\" "/" REL_PATH ${REL_PATH})

        get_filename_component(FILE_NAME ${REL_PATH} NAME)
        get_filename_component(DIR_NAME ${REL_PATH} DIRECTORY)

        # 保留路径结构
        set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders/${DIR_NAME}")
        file(MAKE_DIRECTORY ${OUTPUT_DIR})

        set(OUTPUT_FILE "${OUTPUT_DIR}/${FILE_NAME}.qsb")

        add_custom_command(
                OUTPUT ${OUTPUT_FILE}
                COMMAND ${QSB_EXECUTABLE}
                ${SHADER_FILE}
                -o ${OUTPUT_FILE}
                --glsl 330 --hlsl 50 --msl 12
                -- vulkan
                MAIN_DEPENDENCY ${SHADER_FILE}
                COMMENT "Compiling shader: ${REL_PATH}"
                VERBATIM
                COMMAND_EXPAND_LISTS
        )

        list(APPEND COMPILED_SHADERS ${OUTPUT_FILE})
        message(STATUS "Shader rule: ${SHADER_FILE} -> ${OUTPUT_FILE}")
    endforeach ()

    add_custom_target(ShaderBuild ALL DEPENDS ${COMPILED_SHADERS})
    add_dependencies(Editor ShaderBuild)

    # 创建资源文件内容
    set(QRC_CONTENT "<!DOCTYPE RCC><RCC version=\"1.0\">\n<qresource>\n")
    foreach (OUTPUT_FILE ${COMPILED_SHADERS})
        file(RELATIVE_PATH REL_PATH ${CMAKE_BINARY_DIR} ${OUTPUT_FILE})
        string(REPLACE "\\" "/" REL_PATH ${REL_PATH})
        set(QRC_CONTENT "${QRC_CONTENT}    <file alias=\"${REL_PATH}\">${OUTPUT_FILE}</file>\n")
    endforeach ()
    set(QRC_CONTENT "${QRC_CONTENT}</qresource>\n</RCC>")

    # 写入资源文件
    set(SHADER_QRC "${CMAKE_BINARY_DIR}//shaders.qrc")
    file(WRITE ${SHADER_QRC} ${QRC_CONTENT})

    # 将资源文件添加到Qt资源系统
    qt_add_resources(SHADER_RESOURCE ${SHADER_QRC})
    target_sources(Editor PRIVATE ${SHADER_RESOURCE})
endif ()


target_link_libraries(Editor PRIVATE
        Common
        Engine
        Qt6::Core
        Qt6::Widgets
        Qt6::GuiPrivate
)

target_include_directories(Editor PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
)

target_include_directories(Editor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)

set_target_properties(Editor PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
