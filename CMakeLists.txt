cmake_minimum_required(VERSION 3.15)

project(myx_coroutine LANGUAGES CXX)

set(LIB_NAME ${PROJECT_NAME})

# if (NOT CMAKE_BUILD_TYPE)
#     set(CMAKE_BUILD_TYPE Release)
# endif()

include(cmake/message.cmake)


add_library(${LIB_NAME} STATIC)

# -I
target_include_directories(${LIB_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -l
target_link_libraries(${LIB_NAME}
    PUBLIC

    PRIVATE
)

add_subdirectory(src)
add_subdirectory(example)