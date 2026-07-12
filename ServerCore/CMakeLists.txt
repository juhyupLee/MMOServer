set(SERVERCORE_SOURCES
    Dump/MemoryDump.cpp
    Memory/Global.cpp
    Memory/RingBuffer.cpp
    Network/FrameDecoder.cpp
    Network/JobDispatcher.cpp
    Network/JobQueue.cpp
    Network/NetworkServer.cpp
    Network/NetworkSession.cpp
    Utill/LogManager.cpp
    Utill/Singleton.cpp
    Utill/TimeUtil.cpp
    Utill/UIDGenerator.cpp
)

# The custom allocator remains available to the existing Windows experiments,
# but is intentionally not placed on the Linux server path until its 128-bit
# CAS and address-layout assumptions have their own portability tests.
if(MSVC)
    list(APPEND SERVERCORE_SOURCES
        Memory/MemoryPool.cpp
        PCH/PCH_ServerCore.cpp
    )
endif()

add_library(ServerCore STATIC ${SERVERCORE_SOURCES})

target_include_directories(ServerCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/PCH"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/flatbuffers"
)

target_link_libraries(ServerCore PUBLIC
    ProjectBoostAsio
    ProjectSpdlog
    Threads::Threads
)

target_compile_definitions(ServerCore PUBLIC MMO_CMAKE_BUILD)

if(MSVC)
    target_precompile_headers(ServerCore PRIVATE
        PCH/PCH_ServerCore.h
    )
endif()

if(WIN32)
    target_compile_definitions(ServerCore PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(ServerCore PUBLIC
        ws2_32
        dbghelp
        psapi
        winmm
    )
endif()

if(MSVC)
    target_compile_options(ServerCore PUBLIC /utf-8 /Zc:preprocessor)
endif()

if(BUILD_TESTING)
    add_subdirectory(Test)
endif()
