file(GLOB_RECURSE COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE COMMON_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

filter_auto_generated_files(COMMON_SOURCES)
filter_auto_generated_files(COMMON_HEADERS)

add_library(Common STATIC
        ${COMMON_SOURCES}
        ${COMMON_HEADERS}
)

target_include_directories(Common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Public
)

target_include_directories(Common PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Private
)
