add_executable(FrameDecoderTests
    FrameDecoderTests.cpp
)

target_link_libraries(FrameDecoderTests PRIVATE ServerCore)

add_test(NAME FrameDecoderTests COMMAND FrameDecoderTests)

add_executable(NetworkIntegrationTests
    NetworkIntegrationTests.cpp
)

target_link_libraries(NetworkIntegrationTests PRIVATE ServerCore)

add_test(NAME NetworkIntegrationTests COMMAND NetworkIntegrationTests)
set_tests_properties(NetworkIntegrationTests PROPERTIES TIMEOUT 20)

add_executable(FlowControlTests
    FlowControlTests.cpp
)

target_link_libraries(FlowControlTests PRIVATE ServerCore)

add_test(NAME FlowControlTests COMMAND FlowControlTests)

add_executable(DummyNetworkTests
    DummyNetworkTests.cpp
)

target_link_libraries(DummyNetworkTests PRIVATE ServerCore)

add_test(NAME DummyNetworkTests COMMAND DummyNetworkTests)
set_tests_properties(DummyNetworkTests PROPERTIES TIMEOUT 30)
