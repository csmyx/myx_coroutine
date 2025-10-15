cmake_minimum_required(VERSION 3.21)

project(myx_coroutine LANGUAGES CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
endif()

set(LIB_NAME ${PROJECT_NAME})

# if (NOT CMAKE_BUILD_TYPE)
#     set(CMAKE_BUILD_TYPE Release)
# endif()

include(cmake/message.cmake)
include(cmake/check_tsan_support.cmake)
check_tsan_support()

enable_testing()
find_package(spdlog CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

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
        spdlog::spdlog
    PRIVATE
)

add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(test)