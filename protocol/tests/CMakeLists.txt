add_executable(robot_protocol_tests
        byte_codec_test.cpp
        wheel_velocity_codec_test.cpp
        wheel_velocity_payload_codec_test.cpp
        motion_lifecycle_command_codec_test.cpp
        motion_ack_codec_test.cpp
        motion_response_codec_test.cpp
        link_sync_codec_test.cpp
        heartbeat_codec_test.cpp
        crc16_ccitt_false_test.cpp
        protocol_frame_encoder_test.cpp
        protocol_frame_decoder_test.cpp
        protocol_ingress_router_test.cpp
        motion_transaction_test.cpp
        motion_sequence_test.cpp
        motion_reliable_receiver_test.cpp
)

target_compile_features(
        robot_protocol_tests
        PRIVATE
        cxx_std_20
)

target_link_libraries(robot_protocol_tests
        PRIVATE
        robot_protocol
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(robot_protocol_tests)
