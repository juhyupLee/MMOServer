cmake_minimum_required(VERSION 3.20)
project(MMOServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

option(MMO_ENABLE_ASAN "Enable AddressSanitizer for supported compilers" OFF)
option(MMO_ENABLE_TSAN "Enable ThreadSanitizer for GCC/Clang" OFF)
if(MMO_ENABLE_ASAN AND MMO_ENABLE_TSAN)
    message(FATAL_ERROR "MMO_ENABLE_ASAN and MMO_ENABLE_TSAN cannot be enabled together")
endif()
if(MMO_ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()
if(MMO_ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    else()
        message(FATAL_ERROR "ThreadSanitizer is only configured for GCC/Clang")
    endif()
endif()

# Prefer the developer-local Boost headers under include/boost. They are kept
# out of Git because of their size. A system/vcpkg Boost installation is used
# automatically when the local copy is absent.
add_library(ProjectBoostAsio INTERFACE)
if(EXISTS "${CMAKE_SOURCE_DIR}/include/boost/asio.hpp")
    target_include_directories(ProjectBoostAsio INTERFACE
        "${CMAKE_SOURCE_DIR}/include"
    )
else()
    find_package(Boost 1.83 REQUIRED)
    if(TARGET Boost::headers)
        target_link_libraries(ProjectBoostAsio INTERFACE Boost::headers)
    elseif(TARGET Boost::boost)
        target_link_libraries(ProjectBoostAsio INTERFACE Boost::boost)
    else()
        target_include_directories(ProjectBoostAsio INTERFACE ${Boost_INCLUDE_DIRS})
    endif()
endif()
target_compile_definitions(ProjectBoostAsio INTERFACE
    BOOST_ERROR_CODE_HEADER_ONLY
)

# The repository already carries spdlog's headers.
add_library(ProjectSpdlog INTERFACE)
target_include_directories(ProjectSpdlog INTERFACE
    "${CMAKE_SOURCE_DIR}/include/spdlog/include"
    "${CMAKE_SOURCE_DIR}/include/spdlog/include/spdlog"
)

add_subdirectory(ServerCore)
add_subdirectory(PlayServer)
add_subdirectory(SoakClient)

if(WIN32)
    find_program(MMO_POWERSHELL_EXECUTABLE
        NAMES powershell.exe
        REQUIRED
    )
    add_custom_target(MMOCustomFlatc
        COMMAND
            "${MMO_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/build_custom_flatbuffers.ps1"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Build and promote the MMOServer custom flatc"
    )
endif()
