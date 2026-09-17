add_library(robot_protocol
        byte_codec.c
        wheel_velocity_codec.c
        wheel_velocity_payload_codec.c
        motion_lifecycle_command_codec.c
        motion_ack_codec.c
        motion_response_codec.c
        link_sync_codec.c
        heartbeat_codec.c
        crc16_ccitt_false.c
        protocol_frame_encoder.c
        protocol_frame_decoder.c
        protocol_ingress_router.c
        motion_transaction.c
        motion_sequence.c
        motion_reliable_receiver.c
        motion_lifecycle_coordinator.c
        motion_lifecycle_action.c
)

target_compile_features(robot_protocol PUBLIC c_std_99)

target_include_directories(robot_protocol
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
