add_executable(SoakClient
    main.cpp
)

target_link_libraries(SoakClient PRIVATE ServerCore)

add_executable(ProtocolProbe
    ProtocolProbe.cpp
)

target_link_libraries(ProtocolProbe PRIVATE ServerCore)

if(UNIX)
    target_link_options(SoakClient PRIVATE -rdynamic)
    target_link_options(ProtocolProbe PRIVATE -rdynamic)
endif()
