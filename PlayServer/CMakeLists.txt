add_executable(PlayServer
    main.cpp
    PlayServer.cpp
)

if(MSVC)
    target_sources(PlayServer PRIVATE PCH/PCH_PlayServer.cpp)
endif()

target_include_directories(PlayServer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/PCH"
)

if(MSVC)
    target_precompile_headers(PlayServer PRIVATE
        PCH/PCH_PlayServer.h
    )
endif()

target_link_libraries(PlayServer PRIVATE ServerCore)

if(UNIX)
    target_link_options(PlayServer PRIVATE -rdynamic)
endif()
